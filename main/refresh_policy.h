#pragma once
#include "dashboard.h"

namespace home {
// Absolute wall-clock deadline, always bounded by the next Eastern midnight.
time_t NextLocalDay(time_t now);
time_t NextFootballRefresh(const Football&, time_t now);
time_t NextFormulaOneRefresh(const FormulaOne&, time_t now);
unsigned RetrySeconds(unsigned failures);
// One correction check after first observing all results complete, then daily.
class CompletionCheck {
public:
    time_t Apply(time_t proposed, time_t now, bool completed);
private:
    time_t first_complete_ = 0;
};
}
