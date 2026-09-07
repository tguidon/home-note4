#include "refresh_policy.h"
#include <algorithm>

namespace home {
time_t NextLocalDay(time_t now) {
    tm local{}; localtime_r(&now, &local);
    local.tm_hour = local.tm_min = local.tm_sec = 0;
    ++local.tm_mday; local.tm_isdst = -1;
    return mktime(&local);
}
namespace {
time_t EventDeadline(time_t starts, const std::string& state, bool complete, time_t now) {
    const auto tomorrow = NextLocalDay(now);
    if (!starts || state.empty()) return std::min(tomorrow, now + 300);
    if (complete) return tomorrow;
    if (state == "in") return std::min(tomorrow, now + 300);
    // Unknown/postponed and overdue events remain eligible for bounded retries.
    if (state != "pre" || starts <= now) return std::min(tomorrow, now + 1800);
    if (starts - now <= 1800) return std::min(tomorrow, now + 300);
    if (LocalDateKey(starts) != LocalDateKey(now)) return tomorrow;
    return std::min({tomorrow, now + 3600, starts - 1800});
}
}
time_t NextFootballRefresh(const Football& f, time_t now) {
    if (!f.valid || !f.schedule_complete) return now + 300;
    time_t due = NextLocalDay(now);
    for (const auto& g : f.games)
        due = std::min(due, EventDeadline(ParseUtcTime(g.date), g.state, g.completed, now));
    return due;
}
time_t NextTennisRefresh(const Tennis& t, time_t now) {
    if (!t.valid || t.day != LocalDateKey(now)) return now + 300;
    time_t due = NextLocalDay(now);
    for (const auto& m : t.matches) {
        if (!m.time_valid && !m.completed) due = std::min(due, now + 1800);
        else due = std::min(due, EventDeadline(ParseUtcTime(m.date), m.state, m.completed, now));
    }
    return due;
}
time_t NextFormulaOneRefresh(const FormulaOne& f, time_t now) {
    if (!f.schedule_valid || f.error) return now + 300;
    time_t due = NextLocalDay(now);
    if (f.classification_seen && now < f.classification_seen + 1800)
        due = std::min(due, f.classification_seen + 1800);
    for (const auto& s : f.sessions) {
        if (s.starts > now) due = std::min(due, EventDeadline(s.starts, "pre", false, now));
        else if (s.starts > f.latest.session.starts) {
            // Jolpica has no finality signal. Allow publication across midnight,
            // but do not poll cancelled/unpublished sessions forever at 5 min.
            if (now - s.starts < 12 * 3600) due = std::min(due, now + 300);
            else if (LocalDateKey(s.starts) == LocalDateKey(now)) due = std::min(due, now + 3600);
        } else if (s.starts == f.latest.session.starts && now - s.starts < 4 * 3600) {
            due = std::min(due, now + 300);
        }
    }
    return due;
}
unsigned RetrySeconds(unsigned failures) {
    return std::min(3600u, 60u << std::min(failures ? failures - 1 : 0, 6u));
}
time_t CompletionCheck::Apply(time_t proposed, time_t now, bool completed) {
    if (!completed) first_complete_ = 0;
    else if (!first_complete_) first_complete_ = now;
    if (completed && now < first_complete_ + 1800) return std::min(proposed, first_complete_ + 1800);
    return proposed;
}
}
