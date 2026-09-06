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
void Footer(ZectrixCanvas& c, const std::string& source, time_t stamp, bool error, time_t now, bool refreshing = false, bool more = false) {
    c.Line(14, 245, 386, 245);
    Text(c, 14, 250, refreshing ? (stamp ? "Updating... showing saved data" : "Loading data...") : Updated(stamp, error, now));
    Text(c, 14, 268, source);
    Text(c, 14, 284, more ? "UP/DOWN Sections | OK More | Hold OK Wi-Fi" : "UP/DOWN Sections | OK Refresh | Hold OK Wi-Fi");
}
}
int SectionPages(const Dashboard& d, int section) {
    if (section == 1) return std::max(1, static_cast<int>((d.tennis.matches.size() + 2) / 3));
    if (section == 3) return std::max(1, static_cast<int>((d.nfl.games.size() + 3) / 4));
    return 1;
}
int SectionStart(const Dashboard& d, int section) {
    int page = 0; for (int s = 0; s < section; ++s) page += SectionPages(d, s); return page;
}
int PageCount(const Dashboard& d) { return SectionStart(d, 4); }
void RenderMessage(ZectrixCanvas& c, const char* title, const std::vector<std::string>& lines) {
    c.Clear(); Text(c, 18, 20, "HOME NOTE4", 2); c.Line(18, 58, 382, 58);
    Text(c, 18, 74, title, 2);
    int y = 120; for (const auto& line : lines) { Text(c, 18, y, line); y += 23; }
}
void RenderDashboard(ZectrixCanvas& c, const Dashboard& d, int page, time_t now, bool online, int battery_percent) {
    c.Clear(); char b[100];
    if (now > 1767225600) {
        tm local = {}; localtime_r(&now, &local); strftime(b, sizeof(b), "%a, %b %d, %Y", &local);
    } else snprintf(b, sizeof(b), "Syncing date...");
    Text(c, 14, 12, b);
    const std::string battery = battery_percent < 0 ? "--%" :
        std::to_string(std::clamp(battery_percent, 0, 100)) + "%";
    const std::string status = std::string(online ? "ONLINE  " : "OFFLINE  ") + battery;
    const int status_width = c.TextWidth(status.c_str());
    Text(c, 386 - status_width, 12, status, 1, status_width);
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
            Text(c, 14, 189, "US Open, F1 and NFL: use UP/DOWN.");
        }
        Footer(c, "Weather: Open-Meteo (CC BY 4.0)", d.weather_updated, d.weather_error, now, d.refreshing);
    } else if (page < SectionStart(d, 2)) {
        const int index = page - SectionStart(d, 1);
        const auto& tennis = d.tennis;
        snprintf(b, sizeof(b), "US OPEN / %d OF %d", index + 1, SectionPages(d, 1));
        Text(c, 14, 44, b, 2);
        if (tennis.day && tennis.day != LocalDateKey(now)) {
            snprintf(b, sizeof(b), "Saved %02d/%02d - updating today's matches", (tennis.day / 100) % 100, tennis.day % 100);
        } else snprintf(b, sizeof(b), home::TennisEnded(tennis, now) ? "Season complete | More sports on UP/DOWN" : "Today's singles & doubles | Times in ET");
        Text(c, 14, 77, b);
        if (tennis.matches.empty()) {
            Text(c, 14, 115, TennisEmptyTitle(tennis, now), 2);
            Text(c, 14, 162, d.tennis_error ? "Feed unavailable. Retrying automatically." : home::TennisEnded(tennis, now) ? "Back next season. Dates when available." : "Schedule updates automatically.");
            Text(c, 14, 189, "UP/DOWN for weather, F1 and NFL.");
        }
        const size_t start = index * 3;
        for (size_t i = start; i < std::min(start + 3, tennis.matches.size()); ++i) {
            const auto& m = tennis.matches[i]; const int y = 99 + int(i - start) * 48;
            std::string status = m.state == "in" ? std::string(!online || d.tennis_error || tennis.day != LocalDateKey(now) ? "Saved " : "LIVE ") + m.detail : m.state == "pre" ? (m.time_valid ? LocalGameTime(m.date) : "Time TBA") : m.detail;
            // Division and round remain readable; full time includes the date
            // for a live match carried over from the previous New York day.
            std::string division = m.division == "Men's Singles" ? "MS" : m.division == "Women's Singles" ? "WS" : m.division == "Men's Doubles" ? "MD" : m.division == "Women's Doubles" ? "WD" : "XD";
            Text(c, 14, y, division + " | " + status);
            for (int player = 0; player < 2; ++player) {
                const auto label = std::string(m.winners[player] ? "* " : "") + m.players[player];
                Text(c, 14, y + 15 + player * 15, label, 1, 258);
                Text(c, 282, y + 15 + player * 15, m.scores[player], 1, 104);
            }
        }
        Footer(c, home::TennisEnded(tennis, now) ? "Tennis: ESPN | US Open season complete" : "Tennis: ESPN | * Winner | Updates every 5 min", d.tennis_updated, d.tennis_error, now, d.refreshing, SectionPages(d, 1) > 1);
    } else if (page == SectionStart(d, 2)) {
        Text(c, 14, 44, "FORMULA 1", 2);
        Text(c, 14, 83, d.f1.next_valid ? (d.f1.next.name.empty() ? "No next race announced" : "NEXT: " + d.f1.next.name) : "Next race: waiting for data");
        if (!d.f1.next.name.empty()) Text(c, 14, 105, LocalRaceTime(d.f1.next));
        c.Line(14, 132, 386, 132);
        Text(c, 14, 141, d.f1.last_valid ? (d.f1.last.name.empty() ? "No results this season yet" : "LAST: " + d.f1.last.name) : "Results: waiting for data");
        Text(c, 14, 160, d.f1.last.date);
        for (int i = 0; i < 3; ++i) Text(c, 14, 179 + 20 * i, d.f1.podium[i]);
        const time_t oldest = !d.next_updated || !d.last_updated ? 0 : std::min(d.next_updated, d.last_updated);
        Footer(c, "F1: Jolpica / Ergast | Results, not live timing", oldest, d.next_error || d.last_error, now, d.refreshing);
    } else {
        snprintf(b, sizeof(b), "NFL / %d OF %d", page - SectionStart(d, 3) + 1, SectionPages(d, 3));
        Text(c, 14, 44, b, 2);
        Text(c, 275, 55, d.nfl.week, 1, 108);
        size_t start = static_cast<size_t>(page - SectionStart(d, 3)) * 4;
        if (d.nfl.games.empty()) Text(c, 14, 115, d.nfl.valid ? "No games on the current schedule." : "Waiting for NFL scores and schedule.");
        for (size_t i = start; i < std::min(start + 4, d.nfl.games.size()); ++i) {
            const auto& g = d.nfl.games[i]; int y = 85 + static_cast<int>(i - start) * 39;
            auto matchup = g.away + " @ " + g.home;
            if (g.state == "in" || g.state == "post") matchup = g.away + " " + (g.away_score.empty() ? "-" : g.away_score) + "  -  " + g.home + " " + (g.home_score.empty() ? "-" : g.home_score);
            Text(c, 14, y, matchup);
            Text(c, 14, y + 17, g.state == "pre" ? LocalGameTime(g.date) : g.detail);
        }
        Footer(c, "Scores: ESPN | Polls every 5 min", d.nfl_updated, d.nfl_error, now, d.refreshing, SectionPages(d, 3) > 1);
    }
}
}
