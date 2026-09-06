#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace home::ical {
struct Date {
    int y = 0, m = 0, d = 0, h = 0, min = 0, sec = 0;
    bool day = false, utc = false;
};
int Days(int y, int m, int d);
Date Civil(int days);
int MonthDays(int y, int m);
int Weekday(int days); // Sunday = 0
int64_t Wall(const Date&);
bool ParseDate(const std::string&, Date&);
struct Rule {
    std::string frequency;
    int interval = 1, count = 0, week_start = 1;
    Date until;
    std::vector<int> months, month_days, positions;
    struct Day { int ordinal, weekday; };
    std::vector<Day> weekdays;
};
bool ParseRule(const std::string&, Rule&);
// Callback receives sorted local occurrences. Return false to stop normally.
// Budget is shared across the whole feed to bound CPU work.
bool Expand(const Date&, const Rule&, int first_day, int last_day,
            unsigned& budget, const std::function<bool(const Date&)>&,
            const std::function<bool(const Date&)>& valid_time = {});
}
