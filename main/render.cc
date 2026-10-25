#include "render.h"
#include "battery_indicator.h"
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
    return std::string(error || now - stamp > 30 * 3600 ? "STALE | " : "Updated ") + b;
}
void Footer(ZectrixCanvas& c, const std::string& source, time_t stamp, bool error, time_t now, bool refreshing = false, bool more = false) {
    c.Line(14, 245, 386, 245);
    Text(c, 14, 250, refreshing ? (stamp ? "Updating... showing saved data" : "Loading data...") : Updated(stamp, error, now));
    Text(c, 14, 268, source);
    Text(c, 14, 284, more ? "UP/DOWN Sections | OK More | Hold OK Wi-Fi" : "UP/DOWN Sections | OK Refresh | Hold OK Wi-Fi");
}
}
int SectionPages(const Dashboard& d, int section, time_t /*now*/) {
    if (section == 0) return 1;
    if (section == 1) return std::max(1, static_cast<int>((d.f1.latest.rows.size() + 3) / 4));
    if (section == 2) return std::max(1, static_cast<int>((d.nfl.games.size() + 3) / 4));
    return 1;
}
int SectionStart(const Dashboard& d, int section, time_t now) {
    int page = 0; for (int s = 0; s < section; ++s) page += SectionPages(d, s, now); return page;
}
int PageCount(const Dashboard& d, time_t now) { return SectionStart(d, kSectionCount, now); }
void RenderMessage(ZectrixCanvas& c, const char* title, const std::vector<std::string>& lines) {
    c.Clear(); Text(c, 18, 20, "HOME NOTE4", 2); c.Line(18, 58, 382, 58);
    Text(c, 18, 74, title, 2);
    int y = 120; for (const auto& line : lines) { Text(c, 18, y, line); y += 23; }
}
void RenderDashboard(ZectrixCanvas& c, const Dashboard& d, int page, time_t now, bool online, int battery_percent,
                     const std::string& battery_label) {
    c.Clear(); char b[100];
    if (now > 1767225600) {
        tm local = {}; localtime_r(&now, &local); strftime(b, sizeof(b), "%a, %b %d, %Y", &local);
    } else snprintf(b, sizeof(b), "Syncing date...");
    Text(c, 14, 12, b);
    const std::string battery = !battery_label.empty() ? battery_label : battery_percent < 0 ? "--%" :
        std::to_string(std::clamp(battery_percent, 0, 100)) + "%";
    const bool charging = battery.rfind(kChargingPrefix, 0) == 0;
    const std::string battery_text = charging ? battery.substr(sizeof(kChargingPrefix) - 1) : battery;
    const std::string connection = online ? "ONLINE  " : "OFFLINE  ";
    const int battery_x = 386 - c.TextWidth(battery_text.c_str());
    const int icon_width = charging ? 12 : 0;
    Text(c, battery_x - icon_width - c.TextWidth(connection.c_str()), 12, connection);
    if (charging) {
        // A solid 8 x 12 lightning bolt, aligned with the small status font.
        constexpr uint8_t bolt[] = {0x0c, 0x1c, 0x18, 0x38, 0x30, 0x7e,
                                    0x7e, 0x0c, 0x1c, 0x18, 0x30, 0x20};
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 8; ++x)
                if (bolt[y] & (0x80 >> x)) c.Pixel(battery_x - icon_width + x, 12 + y, true);
    }
    Text(c, battery_x, 12, battery_text);
    c.Line(14, 35, 386, 35);
    if (page < SectionStart(d, 1, now)) {
        const auto& w = d.weather;
        Text(c, 14, 44, "WILTON, CT", 2);
        if (w.valid) {
            snprintf(b, sizeof(b), "%.0f F", w.temperature);
            Text(c, 283, 44, b, 2, 103);
            Text(c, 14, 77, WeatherDescription(w.code), 1, 190);
            snprintf(b, sizeof(b), "Feels %.0f | H %.0f / L %.0f", w.feels, w.high, w.low);
            Text(c, 209, 77, b, 1, 177);
            snprintf(b, sizeof(b), "Wind %.0f mph | Rain %.0f%%", w.wind, w.rain);
            Text(c, 14, 95, b);
        } else Text(c, 14, 80, d.weather_error ? "Weather unavailable - retrying" : "Loading local weather...");
        if (w.valid && (!online || d.weather_error || now - d.weather_updated > 7200)) Text(c, 306, 95, "SAVED", 1, 80);
        if (const auto* forecast = TomorrowForecast(w, now)) {
            snprintf(b, sizeof(b), "Tomorrow: H %.0f / L %.0f F | Rain %.0f%%",
                     forecast->high, forecast->low, forecast->rain);
            Text(c, 14, 113, b);
        } else Text(c, 14, 113, "Tomorrow: forecast unavailable");
        c.Line(14, 134, 386, 134);
        const auto& calendar = d.calendar;
        const auto agenda = SelectAgenda(calendar, now);
        const bool later_days = std::any_of(agenda.events.begin(), agenda.events.end(),
            [now](const auto* event) { return LocalDateKey(event->start) > LocalDateKey(now); });
        const std::string heading = !calendar.configured ? "YOUR CALENDAR" :
            agenda.today ? (later_days ? "TODAY + NEXT" : "TODAY") : "NEXT UP";
        Text(c, 14, 140, heading);
        std::string note = agenda.additional ? "+" + std::to_string(agenda.additional) + " MORE" : "";
        if (calendar.skipped) note = "CHECK CALENDAR";
        if (calendar.configured && (!online || calendar.error)) note += note.empty() ? "SAVED" : " | SAVED";
        Text(c, 386 - c.TextWidth(note.c_str()), 140, note);
        if (!calendar.configured) {
            Text(c, 14, 173, "Add your Google Calendar here.");
            Text(c, 14, 198, "Hold OK to open device setup.");
            Text(c, 14, 223, "Your private calendar stays private.");
        } else if (agenda.events.empty()) {
            Text(c, 14, 174, !calendar.valid ? (calendar.error ? "Calendar unavailable - will retry" : "Loading your calendar...") :
                now >= calendar.through ? "Saved agenda expired - reconnect" : calendar.skipped ? "Some events could not be read" : "No events in the next 90 days.");
            Text(c, 14, 202, calendar.skipped ? "Check Google Calendar for the full agenda." : "Updates every 5 minutes over Wi-Fi.");
        } else {
            for (size_t i = 0; i < agenda.events.size(); ++i) {
                const auto& event = *agenda.events[i]; const int y = 159 + int(i) * 32;
                const bool today = LocalDateKey(event.start) == LocalDateKey(now);
                auto label = CalendarEventTime(event, today, now);
                if (!event.all_day && event.start <= now && event.end > now) label += " | NOW";
                Text(c, 14, y, label);
                Text(c, 14, y + 15, event.title);
            }
        }
        c.Line(14, 260, 386, 260);
        Text(c, 14, 265, Updated(calendar.configured ? calendar.updated : d.weather_updated,
             !online || (calendar.configured ? calendar.error : d.weather_error), now));
        Text(c, 14, 284, "UP/DOWN Sections | OK Refresh | Hold OK Setup");
    } else if (page < SectionStart(d, 2, now)) {
        const auto& f = d.f1;
        const int index = page - SectionStart(d, 1, now);
        Text(c, 14, 44, "FORMULA 1", 2);
        snprintf(b, sizeof(b), "%d OF %d", index + 1, SectionPages(d, 1, now));
        Text(c, 320, 55, b, 1, 66);
        const auto* anchor = F1Anchor(f, now);
        if (anchor) {
            const bool today = LocalDateKey(anchor->starts) == LocalDateKey(now);
            Text(c, 14, 80, std::string(today ? "TODAY: " : "NEXT: ") + anchor->grand_prix);
            Text(c, 14, 99, "Race: " + LocalGameTime(anchor->timestamp));
        } else {
            Text(c, 14, 80, f.schedule_valid ? "Next race schedule pending" : "Race: waiting for schedule");
            Text(c, 14, 99, "Main Grand Prix | Times in Eastern");
        }
        c.Line(14, 119, 386, 119);
        if (f.latest.rows.empty()) {
            Text(c, 14, 132, "Session results are on their way");
            Text(c, 14, 160, f.pending.empty() ? "Waiting for published results." : std::string(F1SessionTitle(f.pending)) + " results pending.");
        } else {
            Text(c, 14, 125, std::string("LATEST: ") + F1SessionTitle(f.latest.session.code));
            tm date{}; localtime_r(&f.latest.session.starts, &date);
            strftime(b, sizeof(b), "%a %m/%d", &date);
            Text(c, 14, 144, F1ShortGrandPrix(f.latest.session.grand_prix) + " | " + b);
            const size_t start = static_cast<size_t>(index) * 4;
            for (size_t i = start; i < std::min(start + 4, f.latest.rows.size()); ++i) {
                const auto& row = f.latest.rows[i]; const int y = 166 + int(i - start) * 18;
                Text(c, 14, y, row.position_text + ". " + row.driver, 1, 245);
                const int width = std::min(116, c.TextWidth(row.time.c_str()));
                Text(c, 386 - width, y, row.time, 1, width);
            }
        }
        const auto source = !f.pending.empty() ? "F1: Jolpica | Awaiting " + f.pending + " results" :
            f.latest.session.code == "Q" || f.latest.session.code == "SQ" ? "Jolpica | Qualifying order; grid may change" : "F1: Jolpica | Published results | Polls 5 min";
        Footer(c, source, f.updated, f.error, now, d.refreshing, SectionPages(d, 1, now) > 1);
    } else {
        snprintf(b, sizeof(b), "NFL / %d OF %d", page - SectionStart(d, 2, now) + 1, SectionPages(d, 2, now));
        Text(c, 14, 44, b, 2);
        Text(c, 275, 55, d.nfl.week, 1, 108);
        size_t start = static_cast<size_t>(page - SectionStart(d, 2, now)) * 4;
        if (d.nfl.games.empty()) Text(c, 14, 115, d.nfl.valid ? "No games on the current schedule." : "Waiting for NFL scores and schedule.");
        for (size_t i = start; i < std::min(start + 4, d.nfl.games.size()); ++i) {
            const auto& g = d.nfl.games[i]; int y = 85 + static_cast<int>(i - start) * 39;
            auto matchup = g.away + " @ " + g.home;
            if (g.state == "in" || g.state == "post") matchup = g.away + " " + (g.away_score.empty() ? "-" : g.away_score) + "  -  " + g.home + " " + (g.home_score.empty() ? "-" : g.home_score);
            Text(c, 14, y, matchup);
            Text(c, 14, y + 17, g.state == "pre" ? LocalGameTime(g.date) : g.detail);
        }
        Footer(c, "Scores: ESPN | 5 min during play", d.nfl_updated, d.nfl_error, now, d.refreshing, SectionPages(d, 2, now) > 1);
    }
}
}
