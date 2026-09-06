#pragma once
#include <array>
#include <ctime>
#include <string>
#include <vector>

namespace home {
struct TennisMatch {
    std::string id, division, round, court, date, state, detail;
    std::array<std::string, 2> players, scores;
    std::array<bool, 2> winners = {};
    bool time_valid = false;
};
struct Tennis {
    bool valid = false, event_found = false;
    int day = 0;
    time_t starts = 0, ends = 0;
    std::vector<TennisMatch> matches;
};
// Today's New York date, including matches still live across midnight.
bool ParseTennis(const std::string& json, time_t now, Tennis& out, const char** error = nullptr);
int LocalDateKey(time_t now);
time_t ParseUtcTime(const std::string& iso);
bool TennisEnded(const Tennis&, time_t now);
std::string TennisEmptyTitle(const Tennis&, time_t now);
}
