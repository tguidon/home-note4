#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

namespace home {
// Rendered as a bitmap by the dashboard because the bundled font is ASCII.
inline constexpr char kChargingPrefix[] = "⚡ ";
// Display smoothing only. Persistent telemetry always retains the raw estimate.
class BatteryIndicator {
public:
    std::string Update(int percent, bool external, bool charging, bool full,
                       bool fault, bool no_battery, int64_t seconds) {
        if (external) {
            was_powered_ = true; settled_after_ = seconds + 30; filtered_ = -1;
            if (fault) return "POWER ERR";
            if (no_battery) return "NO BAT";
            if (full) return "FULL";
            if (charging) return std::string(kChargingPrefix) + (percent < 0 ? "--%" :
                "~" + std::to_string(std::clamp(percent, 0, 100)) + "%");
            return "USB";
        }
        if (was_powered_) { was_powered_ = false; settled_after_ = seconds + 30; }
        if (percent < 0) return "--%";
        if (seconds < settled_after_) return "SETTLING";
        percent = std::clamp(percent, 0, 100);
        if (filtered_ < 0) filtered_ = percent * 100;
        else filtered_ = (filtered_ * 7 + percent * 100) / 8;
        return "~" + std::to_string((filtered_ + 50) / 100) + "%";
    }
private:
    bool was_powered_ = false;
    int64_t settled_after_ = 0;
    int filtered_ = -1;
};
}
