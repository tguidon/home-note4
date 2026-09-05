#include "render.h"
#include <algorithm>
#include <cstdio>

namespace home {
namespace {
void Text(ZectrixCanvas& c, int x, int y, std::string s, int scale = 1, int max_width = 372) {
    // The reference font is ASCII. Keep one replacement per unsupported UTF-8 character.
    std::string ascii;
    for (unsigned char ch : s) {
        if (ch >= 32 && ch < 127) ascii += ch;
        else if (ch >= 192) ascii += '?';
    }
    s = ascii;
    bool clipped = false;
    while (!s.empty() && c.TextWidth(s.c_str(), scale) > max_width) { s.pop_back(); clipped = true; }
    if (clipped) {
        while (!s.empty() && c.TextWidth((s + "...").c_str(), scale) > max_width) s.pop_back();
        s += "...";
    }
    c.Text(x, y, s.c_str(), scale);
}
std::string Updated(time_t stamp, bool error, time_t now) {
    if (!stamp) return error ? "Unavailable - will retry" : "Waiting for data...";
    tm local = {}; localtime_r(&stamp, &local); char b[48];
    strftime(b, sizeof(b), "%m/%d %I:%M%p", &local);
    return std::string(error || now - stamp > 7200 ? "STALE | " : "Updated ") + b;
}
void Footer(ZectrixCanvas& c, const std::string& source, time_t stamp, bool error, time_t now) {
    c.Line(14, 245, 386, 245);
    Text(c, 14, 250, Updated(stamp, error, now));
    Text(c, 14, 268, source);
    Text(c, 14, 284, "UP/DOWN Pages | OK Refresh | Hold OK Wi-Fi");
}
}
int PageCount(const Dashboard& d) { return 2 + std::max(1, static_cast<int>((d.nfl.games.size() + 3) / 4)); }
void RenderMessage(ZectrixCanvas& c, const char* title, const std::vector<std::string>& lines) {
    c.Clear(); Text(c, 18, 20, "HOME NOTE4", 2); c.Line(18, 58, 382, 58);
    Text(c, 18, 74, title, 2);
    int y = 120; for (const auto& line : lines) { Text(c, 18, y, line); y += 23; }
}
void RenderDashboard(ZectrixCanvas& c, const Dashboard& d, int page, time_t now, bool online) {
    c.Clear(); char b[100];
    if (now > 1767225600) {
        tm local = {}; localtime_r(&now, &local); strftime(b, sizeof(b), "%a, %b %d, %Y", &local);
    } else snprintf(b, sizeof(b), "Syncing date...");
    Text(c, 14, 12, b);
    Text(c, 319, 12, online ? "ONLINE" : "OFFLINE", 1, 70);
    c.Line(14, 35, 386, 35);
    if (page == 0) {
        Text(c, 14, 45, "WILTON, CT", 2);
        Text(c, 323, 54, "06897", 1, 64);
        const auto& w = d.weather;
        if (w.valid) {
            snprintf(b, sizeof(b), "%.0f F", w.temperature); Text(c, 14, 88, b, 4);
            Text(c, 14, 157, WeatherDescription(w.code), 2);
            snprintf(b, sizeof(b), "Feels %.0f F   High %.0f / Low %.0f", w.feels, w.high, w.low);
            Text(c, 14, 196, b);
            snprintf(b, sizeof(b), "Wind %.0f mph   Rain chance %.0f%%", w.wind, w.rain);
            Text(c, 14, 221, b);
        } else {
            Text(c, 14, 110, "Weather is on its way", 2);
            Text(c, 14, 162, "Connect to Wi-Fi to load local conditions.");
            Text(c, 14, 189, "F1 and NFL are on the next pages.");
        }
        Footer(c, "Weather: Open-Meteo (CC BY 4.0)", d.weather_updated, d.weather_error, now);
    } else if (page == 1) {
        Text(c, 14, 44, "FORMULA 1", 2);
        Text(c, 14, 83, d.f1.next_valid ? (d.f1.next.name.empty() ? "No next race announced" : "NEXT: " + d.f1.next.name) : "Next race: waiting for data");
        if (!d.f1.next.name.empty()) Text(c, 14, 105, LocalRaceTime(d.f1.next));
        c.Line(14, 132, 386, 132);
        Text(c, 14, 141, d.f1.last_valid ? (d.f1.last.name.empty() ? "No results this season yet" : "LAST: " + d.f1.last.name) : "Results: waiting for data");
        Text(c, 14, 160, d.f1.last.date);
        for (int i = 0; i < 3; ++i) Text(c, 14, 179 + 20 * i, d.f1.podium[i]);
        const time_t oldest = !d.next_updated || !d.last_updated ? 0 : std::min(d.next_updated, d.last_updated);
        Footer(c, "F1: Jolpica / Ergast | Results, not live timing", oldest, d.next_error || d.last_error, now);
    } else {
        snprintf(b, sizeof(b), "NFL / %d OF %d", page - 1, PageCount(d) - 2);
        Text(c, 14, 44, b, 2);
        Text(c, 275, 55, d.nfl.week, 1, 108);
        size_t start = static_cast<size_t>(page - 2) * 4;
        if (d.nfl.games.empty()) Text(c, 14, 115, d.nfl.valid ? "No games on the current schedule." : "Waiting for NFL scores and schedule.");
        for (size_t i = start; i < std::min(start + 4, d.nfl.games.size()); ++i) {
            const auto& g = d.nfl.games[i]; int y = 85 + static_cast<int>(i - start) * 39;
            auto matchup = g.away + " @ " + g.home;
            if (g.state == "in" || g.state == "post") matchup = g.away + " " + (g.away_score.empty() ? "-" : g.away_score) + "  -  " + g.home + " " + (g.home_score.empty() ? "-" : g.home_score);
            Text(c, 14, y, matchup);
            Text(c, 14, y + 17, g.state == "pre" ? LocalGameTime(g.date) : g.detail);
        }
        Footer(c, "Scores: ESPN | Polls every 5 min", d.nfl_updated, d.nfl_error, now);
    }
}
}
