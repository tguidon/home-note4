#include "refresh_policy.h"
#include "battery_indicator.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
using namespace home;
int main() {
    setenv("TZ", kTimezone, 1); tzset();
    const auto now = ParseUtcTime("2026-09-07T16:00Z");
    assert(NextLocalDay(now) == ParseUtcTime("2026-09-08T04:00Z"));
    const auto spring = ParseUtcTime("2026-03-08T05:00Z");
    const auto autumn = ParseUtcTime("2026-11-01T04:00Z");
    assert(NextLocalDay(spring) - spring == 23 * 3600);
    assert(NextLocalDay(autumn) - autumn == 25 * 3600);
    Football nfl; nfl.valid = true;
    assert(NextFootballRefresh(nfl, now) == NextLocalDay(now));
    Game game; game.date = "2026-09-10T20:00Z"; game.state = "pre";
    nfl.games = {game}; assert(NextFootballRefresh(nfl, now) == NextLocalDay(now));
    nfl.games[0].date = "2026-09-07T18:00Z";
    assert(NextFootballRefresh(nfl, now) == now + 3600);
    assert(NextFootballRefresh(nfl, now + 3600) == now + 5400);
    assert(NextFootballRefresh(nfl, now + 5500) == now + 5800);
    nfl.games[0].state = "in";
    assert(NextFootballRefresh(nfl, now) == now + 300);
    nfl.games[0].state = "post"; nfl.games[0].completed = true;
    assert(NextFootballRefresh(nfl, now) == NextLocalDay(now));
    nfl.games[0].completed = false; // Postponed is not a completed result.
    assert(NextFootballRefresh(nfl, now) == now + 1800);
    nfl.games[0].date.clear(); assert(NextFootballRefresh(nfl, now) == now + 300);
    nfl.games.clear(); nfl.schedule_complete = false;
    assert(NextFootballRefresh(nfl, now) == now + 300);
    Football partial;
    assert(ParseFootball(R"({"events":[{"date":"2026-09-10T20:00Z","status":{"type":{"state":"pre"}},"competitions":[{"competitors":[{"homeAway":"home","team":{"abbreviation":"A"}},{"homeAway":"away","team":{"abbreviation":"B"}}]}]},{}]})", partial));
    assert(!partial.schedule_complete);
    CompletionCheck correction;
    const auto tomorrow = NextLocalDay(now);
    assert(correction.Apply(tomorrow, now, true) == now + 1800);
    assert(correction.Apply(tomorrow, now + 300, true) == now + 1800);
    assert(correction.Apply(tomorrow, now + 1800, true) == tomorrow);
    correction.Apply(tomorrow, now + 1900, false);
    assert(correction.Apply(tomorrow, now + 2000, true) == now + 3800);
    Tennis t; t.valid = true; t.day = LocalDateKey(now);
    assert(NextTennisRefresh(t, now) == tomorrow);
    TennisMatch match; match.date = "2026-09-07T20:00Z"; match.state = "pre";
    t.matches = {match}; assert(NextTennisRefresh(t, now) == now + 1800); // Time TBA.
    t.matches[0].time_valid = true;
    assert(NextTennisRefresh(t, now) == now + 3600);
    t.matches[0].state = "in";
    assert(NextTennisRefresh(t, now) == now + 300);
    assert(NextTennisRefresh(t, tomorrow) == tomorrow + 300); // Old day must be fetched.
    FormulaOne f; f.schedule_valid = true;
    f.sessions.push_back({"round", "GP", "FP1", "", now + 7200});
    assert(NextFormulaOneRefresh(f, now) == now + 3600);
    f.sessions[0].code = "SQ";
    assert(NextFormulaOneRefresh(f, now + 6000) == now + 6300);
    assert(NextFormulaOneRefresh(f, now + 7500) == now + 7800);
    f.latest.session = f.sessions[0];
    assert(NextFormulaOneRefresh(f, now + 7500) == now + 7800);
    assert(NextFormulaOneRefresh(f, tomorrow) == NextLocalDay(tomorrow));
    f.latest = {}; f.sessions[0].starts = tomorrow - 3600;
    assert(NextFormulaOneRefresh(f, tomorrow) == tomorrow + 300); // Publication crosses midnight.
    assert(NextFormulaOneRefresh(f, tomorrow + 12 * 3600) == NextLocalDay(tomorrow));
    f.error = true; assert(NextFormulaOneRefresh(f, tomorrow) == tomorrow + 300);
    assert(RetrySeconds(1) == 60 && RetrySeconds(2) == 120 && RetrySeconds(20) == 3600);
    BatteryIndicator battery;
    assert(battery.Update(33, false, false, false, false, false, 100) == "~33%");
    assert(battery.Update(69, true, true, false, false, false, 101) == "CHARGING");
    assert(battery.Update(100, true, false, true, false, false, 102) == "FULL");
    assert(battery.Update(63, false, false, false, false, false, 103) == "SETTLING");
    assert(battery.Update(35, false, false, false, false, false, 134) == "~35%");
    assert(battery.Update(-1, false, false, false, false, false, 135) == "--%");
    assert(battery.Update(80, true, false, false, true, false, 136) == "POWER ERR");
    assert(battery.Update(80, true, false, false, false, true, 137) == "NO BAT");
    std::cout << "PASS: event-aware deadlines, DST/midnight, completion corrections, partial schedules, retries, and battery charging/settling display\n";
}
