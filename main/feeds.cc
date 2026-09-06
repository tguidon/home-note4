#include "feeds.h"
#include "network.h"
#include <atomic>
#include <ctime>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
namespace home {
namespace {
SemaphoreHandle_t lock;
Dashboard shared;
std::atomic<bool> refresh{false};
std::atomic<unsigned> published_revision{0};
constexpr const char* kWeather = "https://api.open-meteo.com/v1/forecast?latitude=41.2018&longitude=-73.4383&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=America%2FNew_York&forecast_days=1";
constexpr const char* kNext = "https://api.jolpi.ca/ergast/f1/current/next.json";
constexpr const char* kLast = "https://api.jolpi.ca/ergast/f1/current/last/results.json?limit=3";
constexpr const char* kNfl = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard?limit=100";
void Publish(Dashboard& d) {
    ++d.revision;
    xSemaphoreTake(lock, portMAX_DELAY); shared = d; xSemaphoreGive(lock);
    published_revision = d.revision;
}
void Run(void*) {
    Dashboard d;
    int64_t weather_due = 0, f1_due = 0, nfl_due = 0, tennis_due = 0, last_manual = -60000000;
    int requested_day = 0;
    // Allocate once before HTTP traffic fragments PSRAM. A growing response
    // string otherwise leaves no contiguous block for the tournament tokens.
    std::string body;
    body.reserve(3 * 1024 * 1024);
    while (true) {
        auto tick = esp_timer_get_time(); time_t now = time(nullptr);
        bool manual = refresh.exchange(false) && tick - last_manual >= 60000000;
        if (manual) { weather_due = f1_due = nfl_due = tennis_due = 0; last_manual = tick; }
        if (WifiConnected() && now >= 1767225600) {
            const int day = LocalDateKey(now);
            if (day != requested_day) { tennis_due = 0; requested_day = day; }
            if (tick >= weather_due || tick >= f1_due || tick >= nfl_due || tick >= tennis_due) {
                d.refreshing = true; Publish(d);
            }
            if (tick >= weather_due) {
                bool ok = FetchJson(kWeather, body) && ParseWeather(body, d.weather);
                d.weather_error = !ok; if (ok) d.weather_updated = time(nullptr);
                weather_due = tick + (ok ? 900LL : 60LL) * 1000000;
                ESP_LOGI("feeds", "weather: %s", ok ? "updated" : "unavailable"); Publish(d);
            }
            if (tick >= f1_due) {
                bool next = FetchJson(kNext, body) && ParseNextRace(body, d.f1);
                d.next_error = !next; if (next) d.next_updated = time(nullptr);
                bool last = FetchJson(kLast, body) && ParseLastRace(body, d.f1);
                d.last_error = !last; if (last) d.last_updated = time(nullptr);
                f1_due = tick + (next && last ? 3600LL : 300LL) * 1000000;
                ESP_LOGI("feeds", "F1 next/results: %s/%s", next ? "updated" : "unavailable", last ? "updated" : "unavailable"); Publish(d);
            }
            if (tick >= nfl_due) {
                bool ok = FetchJson(kNfl, body) && ParseFootball(body, d.nfl);
                d.nfl_error = !ok; if (ok) d.nfl_updated = time(nullptr);
                nfl_due = tick + 300LL * 1000000;
                ESP_LOGI("feeds", "NFL: %s (%u games)", ok ? "updated" : "unavailable", unsigned(d.nfl.games.size())); Publish(d);
            }
            if (tick >= tennis_due) {
                char url[160];
                snprintf(url, sizeof(url), "https://site.api.espn.com/apis/site/v2/sports/tennis/atp/scoreboard?dates=%08d", day);
                const bool fetched = FetchJson(url, body, 3 * 1024 * 1024);
                const char* parse_error = "HTTP request incomplete";
                ESP_LOGI("feeds", "tennis response: %u bytes, fetched=%d, day=%d, largest PSRAM block=%u",
                         unsigned(body.size()), fetched, day, unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
                const bool ok = fetched && ParseTennis(body, now, d.tennis, &parse_error);
                if (!ok) ESP_LOGW("feeds", "tennis failure: %s", parse_error);
                d.tennis_error = !ok; if (ok) d.tennis_updated = time(nullptr);
                const int seconds = !ok ? 300 : d.tennis.event_found && now <= d.tennis.ends ? 300 : 21600;
                tennis_due = esp_timer_get_time() + int64_t(seconds) * 1000000;
                ESP_LOGI("feeds", "US Open: %s (%u matches, day %d)", ok ? "updated" : "unavailable", unsigned(d.tennis.matches.size()), d.tennis.day);
            }
            if (d.refreshing) { d.refreshing = false; Publish(d); }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}
void StartFeeds() {
    lock = xSemaphoreCreateMutex(); configASSERT(lock);
    const BaseType_t started = xTaskCreate(Run, "dashboard_feeds", 16384, nullptr, 1, nullptr);
    configASSERT(started == pdPASS);
}
Dashboard Snapshot() { xSemaphoreTake(lock, portMAX_DELAY); Dashboard d = shared; xSemaphoreGive(lock); return d; }
unsigned FeedRevision() { return published_revision.load(); }
void RefreshFeeds() { refresh = true; }
}
