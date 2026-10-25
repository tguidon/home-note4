#include "feeds.h"
#include "network.h"
#include "refresh_policy.h"
#include "app_events.h"
#include <algorithm>
#include <climits>
#include <atomic>
#include <ctime>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
namespace home {
namespace {
SemaphoreHandle_t lock;
Dashboard shared;
std::atomic<bool> refresh{false};
TaskHandle_t feed_task = nullptr;
std::atomic<unsigned> published_revision{0};
constexpr const char* kWeather = "https://api.open-meteo.com/v1/forecast?latitude=41.2018&longitude=-73.4383&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=America%2FNew_York&forecast_days=2";
constexpr const char* kNfl = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard?limit=100";
void Publish(Dashboard& d) {
    ++d.revision;
    xSemaphoreTake(lock, portMAX_DELAY); shared = d; xSemaphoreGive(lock);
    published_revision = d.revision;
    WakeApp();
}
struct Deadline {
    int64_t at = 0;
    unsigned failures = 0;
    void Schedule(bool ok, time_t due) {
        const time_t now = time(nullptr);
        unsigned delay = ok ? static_cast<unsigned>(std::clamp<time_t>(due - now, 1, 90000))
                            : RetrySeconds(++failures);
        if (ok) failures = 0;
        at = esp_timer_get_time() + int64_t(delay) * 1000000;
    }
};
void Run(void*) {
    Dashboard d;
    const std::string calendar_url = CalendarUrl();
    d.calendar.configured = !calendar_url.empty();
    Publish(d);
    Deadline weather, f1, nfl, calendar;
    CompletionCheck nfl_completion;
    int64_t last_manual = -60000000;
    int requested_day = 0;
    std::string body;
    // Calendar ICS may need a large contiguous buffer; reserve before TLS traffic.
    body.reserve(calendar_url.empty() ? 768 * 1024 : 3 * 1024 * 1024);
    while (true) {
        const auto tick = esp_timer_get_time();
        const time_t now = time(nullptr);
        const bool manual = refresh.exchange(false) && tick - last_manual >= 60000000;
        if (manual) {
            weather.at = f1.at = nfl.at = calendar.at = 0;
            last_manual = tick;
        }
        if (WifiConnected() && now >= 1767225600) {
            const int day = LocalDateKey(now);
            if (day != requested_day) {
                weather.at = f1.at = nfl.at = calendar.at = 0;
                requested_day = day;
            }
            bool changed = false;
            if (tick >= weather.at) {
                const bool first = !d.weather.valid;
                const bool ok = FetchJson(kWeather, body) && ParseWeather(body, d.weather);
                d.weather_error = !ok; if (ok) d.weather_updated = time(nullptr);
                weather.Schedule(ok, time(nullptr) + 900);
                ESP_LOGI("feeds", "weather: %s", ok ? "updated" : "unavailable");
                if (first) Publish(d); // First usable home screen need not wait for sports.
                changed = true;
            }
            if (!calendar_url.empty() && tick >= calendar.at) {
                const bool first = !d.calendar.valid;
                const bool ok = FetchCalendar(calendar_url, body) && ParseCalendar(body, time(nullptr), d.calendar);
                d.calendar.error = !ok;
                calendar.Schedule(ok, time(nullptr) + 300);
                ESP_LOGI("feeds", "calendar: %s, skipped=%u", ok ? "updated" : "unavailable", d.calendar.skipped);
                if (first) Publish(d);
                changed = true;
            }
            if (tick >= f1.at) {
                RefreshFormulaOne(d.f1, time(nullptr), body, [](const char* url, std::string& response) {
                    return FetchJson(url, response);
                });
                f1.Schedule(!d.f1.error, NextFormulaOneRefresh(d.f1, time(nullptr)));
                ESP_LOGI("feeds", "F1: %s; next check in %lld seconds", d.f1.error ? "unavailable" : "updated",
                         (f1.at - esp_timer_get_time()) / 1000000);
                changed = true;
            }
            if (tick >= nfl.at) {
                const bool ok = FetchJson(kNfl, body) && ParseFootball(body, d.nfl);
                d.nfl_error = !ok; if (ok) d.nfl_updated = time(nullptr);
                bool complete = false;
                for (const auto& game : d.nfl.games) {
                    if (LocalDateKey(ParseUtcTime(game.date)) == day && game.completed) complete = true;
                    if (!game.completed && (game.state == "in" || LocalDateKey(ParseUtcTime(game.date)) == day)) { complete = false; break; }
                }
                auto due = NextFootballRefresh(d.nfl, time(nullptr));
                if (ok) due = nfl_completion.Apply(due, time(nullptr), complete);
                nfl.Schedule(ok, due);
                ESP_LOGI("feeds", "NFL: %s; next check in %lld seconds", ok ? "updated" : "unavailable",
                         (nfl.at - esp_timer_get_time()) / 1000000);
                changed = true;
            }
            // One publication per completed batch avoids redundant updating frames.
            if (changed) Publish(d);
        }
        int64_t wait_us = 30000000; // Also covers initial time sync / reconnect.
        if (WifiConnected() && now >= 1767225600) {
            int64_t earliest = std::min({weather.at, f1.at, nfl.at,
                                       calendar_url.empty() ? INT64_MAX : calendar.at});
            earliest = std::min(earliest, esp_timer_get_time() + int64_t(NextLocalDay(time(nullptr)) - time(nullptr)) * 1000000);
            wait_us = std::clamp<int64_t>(earliest - esp_timer_get_time(), 1000, 60000000);
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(std::max<int64_t>(1, wait_us / 1000)));
    }
}
}
void StartFeeds() {
    lock = xSemaphoreCreateMutex(); configASSERT(lock);
    const BaseType_t started = xTaskCreate(Run, "dashboard_feeds", 16384, nullptr, 1, &feed_task);
    configASSERT(started == pdPASS);
}
Dashboard Snapshot() { xSemaphoreTake(lock, portMAX_DELAY); Dashboard d = shared; xSemaphoreGive(lock); return d; }
unsigned FeedRevision() { return published_revision.load(); }
void RefreshFeeds() { refresh = true; if (feed_task) xTaskNotifyGive(feed_task); }
}
