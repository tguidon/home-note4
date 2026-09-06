#include "dashboard.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include "cJSON.h"

namespace home {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json Parse(const std::string& s) { return Json(cJSON_Parse(s.c_str()), cJSON_Delete); }
const cJSON* Get(const cJSON* p, const char* k) { return cJSON_GetObjectItemCaseSensitive(p, k); }
std::string Str(const cJSON* p) { return cJSON_IsString(p) && p->valuestring ? p->valuestring : ""; }
std::string Str(const cJSON* p, const char* k) { return Str(Get(p, k)); }
bool Number(const cJSON* p, double& out) {
    if (!cJSON_IsNumber(p) || !std::isfinite(p->valuedouble)) return false;
    out = p->valuedouble; return true;
}
bool Daily(const cJSON* p, const char* k, double& out) {
    return Number(cJSON_GetArrayItem(Get(p, k), 0), out);
}
int Priority(const Game& g) { return g.state == "in" ? 0 : g.state == "pre" ? 1 : 2; }
}
bool ParseWeather(const std::string& s, Weather& out) {
    auto j = Parse(s); if (!j) return false;
    const auto* c = Get(j.get(), "current"); const auto* d = Get(j.get(), "daily");
    Weather w; double code;
    if (!Number(Get(c, "temperature_2m"), w.temperature) ||
        !Number(Get(c, "apparent_temperature"), w.feels) ||
        !Number(Get(c, "wind_speed_10m"), w.wind) ||
        !Number(Get(c, "weather_code"), code) ||
        !Daily(d, "temperature_2m_max", w.high) || !Daily(d, "temperature_2m_min", w.low) ||
        !Daily(d, "precipitation_probability_max", w.rain)) return false;
    w.observed = Str(c, "time");
    if (w.observed.empty() || code < 0 || code > 99) return false;
    w.code = static_cast<int>(code); w.valid = true; out = w; return true;
}
bool ParseFootball(const std::string& s, Football& out) {
    auto j = Parse(s); if (!j) return false;
    const auto* events = Get(j.get(), "events"); if (!cJSON_IsArray(events)) return false;
    Football f; double week = 0;
    if (Number(Get(Get(j.get(), "week"), "number"), week)) f.week = "Week " + std::to_string(static_cast<int>(week));
    const cJSON* e;
    cJSON_ArrayForEach(e, events) {
        const auto* comp = cJSON_GetArrayItem(Get(e, "competitions"), 0);
        const auto* competitors = Get(comp, "competitors");
        const auto* status = Get(Get(e, "status"), "type");
        Game g; g.state = Str(status, "state"); g.detail = Str(status, "shortDetail"); g.date = Str(e, "date");
        if (g.state.empty()) continue;
        const cJSON* team;
        cJSON_ArrayForEach(team, competitors) {
            auto abbreviation = Str(Get(team, "team"), "abbreviation");
            auto score = Str(team, "score");
            if (Str(team, "homeAway") == "home") { g.home = abbreviation; g.home_score = score; }
            if (Str(team, "homeAway") == "away") { g.away = abbreviation; g.away_score = score; }
        }
        if (g.away.empty() || g.home.empty()) continue;
        f.games.push_back(g);
        if (f.games.size() >= 64) break;
    }
    // An empty schedule is valid. An unrecognizable nonempty feed is not.
    if (cJSON_GetArraySize(events) > 0 && f.games.empty()) return false;
    std::stable_sort(f.games.begin(), f.games.end(), [](const Game& a, const Game& b) {
        if (Priority(a) != Priority(b)) return Priority(a) < Priority(b);
        return a.state == "post" ? a.date > b.date : a.date < b.date;
    });
    f.valid = true; out = f; return true;
}
const char* WeatherDescription(int c) {
    if (c == 0) return "Clear skies";
    if (c == 1) return "Mostly clear";
    if (c == 2) return "Partly cloudy";
    if (c == 3) return "Overcast";
    if (c == 45 || c == 48) return "Foggy";
    if (c >= 51 && c <= 57) return "Drizzle";
    if (c >= 61 && c <= 67) return "Rain";
    if (c >= 71 && c <= 77) return "Snow";
    if (c >= 80 && c <= 82) return "Rain showers";
    if (c == 85 || c == 86) return "Snow showers";
    if (c >= 95) return "Thunderstorms";
    return "Conditions unavailable";
}
time_t ParseUtcTime(const std::string& iso) {
    tm utc = {};
    const char* end = strptime(iso.c_str(), "%Y-%m-%dT%H:%M", &utc);
    if (!end || iso.back() != 'Z') return 0;
    // ESP-IDF's newlib does not expose timegm. Convert UTC without changing
    // the process timezone, which is also used by the display task.
    const int year = utc.tm_year + 1900;
    if (year < 1970 || year > 2100 || utc.tm_mon < 0 || utc.tm_mon > 11) return 0;
    auto leap = [](int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
    const int months[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (utc.tm_mday < 1 || utc.tm_mday > months[utc.tm_mon] + (utc.tm_mon == 1 && leap(year))) return 0;
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += leap(y) ? 366 : 365;
    for (int m = 0; m < utc.tm_mon; ++m) days += months[m] + (m == 1 && leap(year));
    days += utc.tm_mday - 1;
    time_t t = static_cast<time_t>(days * 86400 + utc.tm_hour * 3600 + utc.tm_min * 60);
    return t;
}
std::string LocalGameTime(const std::string& iso) {
    const time_t t = ParseUtcTime(iso);
    if (!t) return "Time TBA";
    tm local = {}; localtime_r(&t, &local);
    char b[48]; strftime(b, sizeof(b), "%a %m/%d %I:%M%p %Z", &local); return b;
}
bool ValidCredentials(const std::string& ssid, const std::string& password) {
    return !ssid.empty() && ssid.size() <= 32 && ssid.find('\0') == std::string::npos &&
        password.find('\0') == std::string::npos && (password.empty() || (password.size() >= 8 && password.size() <= 63));
}
}
