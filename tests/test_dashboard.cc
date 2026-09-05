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
    assert(home::ParseNextRace(Load("f1-next"), d.f1));
    assert(home::ParseLastRace(Load("f1-last"), d.f1));
    assert(!d.f1.next.name.empty() && !d.f1.podium[0].empty());
    auto saved_name = d.f1.next.name;
    assert(!home::ParseNextRace("<html>error</html>", d.f1));
    assert(d.f1.next.name == saved_name);
    home::FormulaOne empty;
    assert(home::ParseNextRace(R"({"MRData":{"RaceTable":{"Races":[]}}})", empty));
    assert(empty.next_valid && empty.next.name.empty());
    assert(!home::ParseLastRace(R"({"MRData":{"RaceTable":{"Races":[{}]}}})", empty));
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
    if (argc > 1) {
        auto now = static_cast<time_t>(1788645600);
        d.weather_updated = d.next_updated = d.last_updated = d.nfl_updated = now;
        ZectrixCanvas c;
        for(int page=0; page<home::PageCount(d); ++page) {
            home::RenderDashboard(c,d,page,now,true);
            Pbm(c, std::string(argv[1]) + "/page-" + std::to_string(page) + ".pbm");
        }
        home::RenderMessage(c,"Let's get connected",{"1. Join Wi-Fi: HomeNote4-ABCD","   Password: example12345","2. Open http://192.168.4.1","3. Enter your home 2.4 GHz Wi-Fi.","Keep this screen's password private.","Hold DOWN to power off."});
        Pbm(c, std::string(argv[1]) + "/setup.pbm");
        d.weather_error = true;
        home::RenderDashboard(c,d,0,now + 86400,false);
        Pbm(c, std::string(argv[1]) + "/offline.pbm");
    }
    std::cout << "PASS: real API fixtures, failure preservation, empty feeds, team mapping, live priority, DST, credentials, and pagination\n";
}
