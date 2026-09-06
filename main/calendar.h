#pragma once
#include <ctime>
#include <string>
#include <vector>
namespace home {
constexpr size_t kCalendarUrlLimit = 1024;
struct CalendarEvent {
    std::string title;
    time_t start = 0, end = 0;
    bool all_day = false;
};
struct Calendar {
    bool configured = false, valid = false, error = false;
    unsigned skipped = 0;
    time_t updated = 0, through = 0;
    std::vector<CalendarEvent> events;
};
struct Agenda {
    bool today = false;
    std::vector<const CalendarEvent*> events;
};
// Accept only Google's private ICS endpoint. The URL is a credential.
bool ValidCalendarUrl(const std::string&);
// Parse atomically, expanding a bounded 90-day window. Unsupported events are
// counted and shown as a warning; malformed/truncated feeds preserve prior data.
bool ParseCalendar(const std::string&, time_t now, Calendar&);
Agenda SelectAgenda(const Calendar&, time_t now);
std::string CalendarEventTime(const CalendarEvent&, bool today, time_t now = time(nullptr));
}
