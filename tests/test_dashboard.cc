#include "dashboard.h"
#include "render.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

std::string Load(const std::string& name) {
    std::ifstream f("tests/fixtures/" + name + ".json"); assert(f.good());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void Pbm(ZectrixCanvas& c, const std::string& filename) {
    std::ofstream f(filename, std::ios::binary); f << "P4\n400 300\n";
    for (size_t i=0; i<c.size(); ++i) f.put(static_cast<char>(~c.data()[i]));
}
int main(int argc, char** argv) {
    setenv("TZ", home::kTimezone, 1); tzset();
    home::Dashboard d;
    assert(home::ParseWeather(Load("weather"), d.weather));
    assert(d.weather.valid && d.weather.temperature == 74.0);
    assert(!home::ParseWeather("{}", d.weather) && d.weather.temperature == 74.0);
    assert(!home::ParseWeather(R"({"current":{"temperature_2m":null}})", d.weather));
    assert(home::ParseFootball(Load("nfl"), d.nfl));
    assert(d.nfl.valid && !d.nfl.games.empty());
    auto n = d.nfl.games.size();
    assert(!home::ParseFootball(R"({"events":[{}]})", d.nfl));
    assert(d.nfl.games.size() == n);
    home::Football none;
    assert(home::ParseFootball(R"({"events":[]})", none) && none.games.empty());
    home::Football mixed;
    assert(home::ParseFootball(R"({"events":[
      {"date":"2026-09-06T17:00Z","status":{"type":{"state":"pre"}},"competitions":[{"competitors":[{"homeAway":"home","team":{"abbreviation":"NYG"},"score":"0"},{"homeAway":"away","team":{"abbreviation":"DAL"},"score":"0"}]}]},
      {"date":"2026-09-05T17:00Z","status":{"type":{"state":"in","shortDetail":"Q3"}},"competitions":[{"competitors":[{"homeAway":"away","team":{"abbreviation":"NE"},"score":"7"},{"homeAway":"home","team":{"abbreviation":"NYJ"},"score":"14"}]}]}
    ]})", mixed));
    assert(mixed.games.front().away == "NE" && mixed.games.front().home_score == "14");
    assert(mixed.games.back().home == "NYG");
    assert(home::LocalGameTime("2026-09-06T13:00:00Z") == "Sun 09/06 09:00AM EDT");
    assert(home::LocalGameTime("2026-01-04T13:00Z") == "Sun 01/04 08:00AM EST");
    assert(home::LocalGameTime("") == "Time TBA");
    assert(home::ValidCredentials("Home Wi-Fi", "my password"));
    assert(home::ValidCredentials(std::string(32,'a'), ""));
    assert(!home::ValidCredentials(std::string(33,'a'), "password"));
    assert(!home::ValidCredentials("", "password"));
    assert(!home::ValidCredentials("wifi", "short"));
    assert(!home::ValidCredentials("wifi", std::string(64,'a')));
    assert(!home::ValidCredentials(std::string("a\0b",3), "password"));
    assert(home::WeatherDescription(95) == std::string("Thunderstorms"));
    assert(home::PageCount(d) == 3 + static_cast<int>((n + 3) / 4));
    const auto tennis_now = home::ParseUtcTime("2026-09-05T22:00Z");
    assert(home::LocalDateKey(home::ParseUtcTime("2026-09-06T02:00Z")) == 20260905);
    assert(home::LocalDateKey(home::ParseUtcTime("2026-09-06T04:00Z")) == 20260906);
    assert(home::ParseTennis(Load("tennis"), tennis_now, d.tennis));
    assert(d.tennis.valid && d.tennis.event_found && !d.tennis.matches.empty());
    assert(d.tennis.matches.front().state == "in");
    bool doubles = false, result = false;
    for (const auto& match : d.tennis.matches) {
        doubles |= match.division.find("Doubles") != std::string::npos;
        result |= match.winners[0] || match.winners[1];
        assert(home::LocalDateKey(home::ParseUtcTime(match.date)) == 20260905 || match.state == "in");
    }
    assert(doubles && result);
    const auto saved_matches = d.tennis.matches.size();
    assert(!home::ParseTennis("{", tennis_now, d.tennis));
    assert(!home::ParseTennis(R"({"events":[{"name":"US Open"}]})", tennis_now, d.tennis));
    assert(d.tennis.matches.size() == saved_matches);
    home::Tennis ended = d.tennis;
    const auto after_open = home::ParseUtcTime("2026-09-14T12:00Z");
    assert(home::ParseTennis(R"({"events":[]})", after_open, ended));
    assert(ended.matches.empty() && home::TennisEmptyTitle(ended, after_open) == "US Open is over");
    assert(home::TennisEmptyTitle(home::Tennis{}, after_open) == "US Open is over");
    home::Tennis empty_today;
    assert(home::ParseTennis(R"({"events":[]})", tennis_now, empty_today));
    assert(home::TennisEmptyTitle(empty_today, tennis_now) == "US Open is not in session");
    assert(home::ParseFootball(Load("nfl-empty"), none) && none.games.empty());
    assert(home::SectionStart(d, 0) == 0 && home::SectionStart(d, 1) == 1);
    assert(home::SectionStart(d, 2) == 1 + home::SectionPages(d, 1));
    assert(home::SectionStart(d, 3) == 2 + home::SectionPages(d, 1));
    assert(home::PageCount(d) == home::SectionStart(d, 3) + home::SectionPages(d, 3));
    if (argc > 2) {
        std::ifstream source(argv[2]); assert(source.good());
        std::string payload{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
        home::Tennis full;
        assert(home::ParseTennis(payload, tennis_now, full));
        assert(full.matches.size() > 20);
        std::cout << "Full public tournament response: " << payload.size() << " bytes, " << full.matches.size() << " New York day/live matches\n";
    }
    if (argc > 1) {
        auto now = static_cast<time_t>(1788645600);
        d.weather_updated = d.nfl_updated = d.tennis_updated = now;
        ZectrixCanvas c;
        for(int page=0; page<home::PageCount(d); ++page) {
            home::RenderDashboard(c,d,page,now,true,87);
            Pbm(c, std::string(argv[1]) + "/page-" + std::to_string(page) + ".pbm");
        }
        auto finished = d; finished.tennis = ended;
        home::RenderDashboard(c,finished,1,after_open,true,87);
        Pbm(c, std::string(argv[1]) + "/tennis-ended.pbm");
        auto loading = d; loading.tennis = {}; loading.tennis_updated = 0; loading.refreshing = true;
        home::RenderDashboard(c,loading,1,now,true,87);
        Pbm(c, std::string(argv[1]) + "/tennis-loading.pbm");
        auto retry = d; retry.tennis_error = true; retry.tennis.day = 20260904;
        home::RenderDashboard(c,retry,1,now,true,87);
        Pbm(c, std::string(argv[1]) + "/tennis-stale.pbm");
        home::RenderMessage(c,"Let's get connected",{"1. Join Wi-Fi: HomeNote4-ABCD","   Password: example12345","2. Open http://192.168.4.1","3. Enter your home 2.4 GHz Wi-Fi.","Keep this screen's password private.","Hold DOWN to power off."});
        Pbm(c, std::string(argv[1]) + "/setup.pbm");
        d.weather_error = true;
        home::RenderDashboard(c,d,0,now + 86400,false,100);
        Pbm(c, std::string(argv[1]) + "/offline.pbm");
        home::RenderDashboard(c,d,0,now,true,0);
        Pbm(c, std::string(argv[1]) + "/battery-empty.pbm");
        home::RenderDashboard(c,d,0,now,false);
        Pbm(c, std::string(argv[1]) + "/battery-unknown.pbm");
    }
    std::cout << "PASS: real API fixtures, failure preservation, empty feeds, team mapping, live priority, DST, credentials, and pagination\n";
}
