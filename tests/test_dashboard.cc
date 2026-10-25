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
    const auto forecast_now = home::ParseUtcTime("2026-09-05T16:00:00Z");
    assert(!home::TomorrowForecast(d.weather, forecast_now)); // One-day response remains usable.
    assert(home::ParseWeather(Load("weather-two-days"), d.weather));
    const auto* tomorrow = home::TomorrowForecast(d.weather, forecast_now);
    assert(tomorrow && tomorrow->high == 78.2 && tomorrow->low == 61.4 && tomorrow->rain == 40);
    assert(!home::TomorrowForecast(d.weather, home::ParseUtcTime("2026-09-06T04:00:00Z")));
    assert(!home::TomorrowForecast(d.weather, 0));
    auto incomplete = Load("weather-two-days");
    const auto rain = incomplete.find("\n      40"); assert(rain != std::string::npos);
    incomplete.replace(rain, 9, "\n      null");
    home::Weather partial;
    assert(home::ParseWeather(incomplete, partial) && partial.valid && !partial.tomorrow.valid);
    auto dst = d.weather;
    dst.tomorrow.date = "2026-11-02";
    assert(home::TomorrowForecast(dst, home::ParseUtcTime("2026-11-01T04:30:00Z"))); // 25-hour day.
    dst.tomorrow.date = "2026-03-09";
    assert(home::TomorrowForecast(dst, home::ParseUtcTime("2026-03-08T05:30:00Z"))); // 23-hour day.
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
    assert(home::PageCount(d) == 2 + static_cast<int>((n + 3) / 4));
    assert(home::LocalDateKey(home::ParseUtcTime("2026-09-06T02:00Z")) == 20260905);
    assert(home::LocalDateKey(home::ParseUtcTime("2026-09-06T04:00Z")) == 20260906);
    assert(home::ParseFootball(Load("nfl-empty"), none) && none.games.empty());
    assert(home::SectionStart(d, 0) == 0 && home::SectionStart(d, 1) == 1);
    assert(home::SectionStart(d, 2) == 1 + home::SectionPages(d, 1));
    assert(home::PageCount(d) == home::SectionStart(d, 2) + home::SectionPages(d, 2));
    assert(home::NextSection(0, false) == 1 && home::NextSection(1, false) == 2);
    assert(home::NextSection(2, false) == 0 && home::NextSection(0, true) == 2);
    assert(home::NextSection(2, true) == 1 && home::NextSection(1, true) == 0);
    if (argc > 1) {
        auto now = static_cast<time_t>(1788645600);
        d.weather_updated = d.nfl_updated = now;
        ZectrixCanvas c;
        for(int page=0; page<home::PageCount(d); ++page) {
            home::RenderDashboard(c,d,page,now,true,87);
            Pbm(c, std::string(argv[1]) + "/page-" + std::to_string(page) + ".pbm");
        }
        home::RenderMessage(c,"Let's get connected",{"1. Join Wi-Fi: HomeNote4-ABCD","   Password: example12345","2. Open http://192.168.4.1","3. Enter your home 2.4 GHz Wi-Fi.","Keep this screen's password private.","Hold DOWN to power off."});
        Pbm(c, std::string(argv[1]) + "/setup.pbm");
        d.weather_error = true;
        home::RenderDashboard(c,d,0,now + 86400,false,100);
        Pbm(c, std::string(argv[1]) + "/offline.pbm");
        home::RenderDashboard(c,d,0,now,true,0);
        Pbm(c, std::string(argv[1]) + "/battery-empty.pbm");
        home::RenderDashboard(c,d,0,now,false);
        Pbm(c, std::string(argv[1]) + "/battery-unknown.pbm");
        for (const auto* label : {"⚡ ~69%", "⚡ ~99%", "⚡ --%", "100% FULL", "SETTLING", "~33%", "POWER ERR"}) {
            home::RenderDashboard(c,d,0,now,true,-1,label);
            Pbm(c, std::string(argv[1]) + "/power-" + label + ".pbm");
        }
    }
    std::cout << "PASS: real API fixtures, failure preservation, empty feeds, team mapping, live priority, DST, credentials, and pagination\n";
}
