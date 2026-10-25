#pragma once
#include <ctime>
#include <string>
namespace home {
int LocalDateKey(time_t now);
time_t ParseUtcTime(const std::string& iso);
}
