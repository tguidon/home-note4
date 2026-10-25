#pragma once
#include <cstddef>

namespace home {
// Provisional voltage-to-remaining-runtime curve for this device and workload.
// Lower knots use the Sept 7–15, 2026 run; the overwritten upper range is an
// interpolation to the existing full-voltage anchor, not measured capacity.
// See docs/battery-review-2026-09-15.md. When used during charging this is
// only a voltage proxy, capped below 100 until the charger reports completion.
inline int DischargePercent(int millivolts) {
    if (millivolts <= 0) return -1;
    struct Point { int mv, percent; };
    constexpr Point curve[] = {{3000,0}, {3200,1}, {3300,3}, {3350,4},
        {3400,9}, {3450,15}, {3500,20}, {3550,30}, {3600,45},
        {3650,55}, {3700,62}, {3720,65}, {4144,100}};
    if (millivolts <= curve[0].mv) return 0;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (millivolts <= curve[i].mv) {
            const auto& lo = curve[i-1]; const auto& hi = curve[i];
            const int span = hi.mv - lo.mv;
            return lo.percent + ((millivolts - lo.mv) * (hi.percent - lo.percent) + span/2) / span;
        }
    }
    return 100;
}
inline constexpr int kBatterySampleSeconds = 1800;
}
