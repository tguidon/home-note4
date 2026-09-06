#pragma once
#include "tennis.h"
#include "formula_one.h"
#include <ctime>
#include <string>
#include <vector>

namespace home {
constexpr const char* kTimezone = "EST5EDT,M3.2.0,M11.1.0";
struct Weather {
    bool valid = false;
    double temperature = 0, feels = 0, high = 0, low = 0, wind = 0, rain = 0;
    int code = 0;
    std::string observed;
};
struct Game {
    std::string away, home, away_score, home_score, state, detail, date;
};
struct Football {
    bool valid = false;
    std::string week;
    std::vector<Game> games;
};
struct Dashboard {
    Weather weather;
    FormulaOne f1;
    Football nfl;
    Tennis tennis;
    time_t tennis_updated = 0;
    bool tennis_error = false, refreshing = false;
    time_t weather_updated = 0, nfl_updated = 0;
    bool weather_error = false, nfl_error = false;
    unsigned revision = 0;
};
bool ParseWeather(const std::string&, Weather&);
bool ParseFootball(const std::string&, Football&);
const char* WeatherDescription(int code);
std::string LocalGameTime(const std::string& iso);
bool ValidCredentials(const std::string& ssid, const std::string& password);
}
