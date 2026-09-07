#pragma once
#include <ctime>
#include <string>
#include <vector>

namespace home {
struct F1Session {
    std::string round_id, grand_prix, code, timestamp;
    time_t starts = 0;
};
struct F1Row {
    int position = 0;
    std::string position_text, driver, time;
};
struct F1Result {
    F1Session session;
    std::vector<F1Row> rows;
};
struct FormulaOne {
    bool schedule_valid = false, error = false;
    int year = 0;
    time_t schedule_updated = 0, updated = 0;
    time_t classification_seen = 0;
    std::vector<F1Session> sessions;
    F1Result latest;
    std::string pending;
};
enum class F1ResultState { Invalid, Pending, Published };
bool ParseF1Schedule(const std::string&, int year, std::vector<F1Session>&);
F1ResultState ParseF1Result(const std::string&, const F1Session&, F1Result&);
const F1Session* F1Anchor(const FormulaOne&, time_t now);
const char* F1SessionTitle(const std::string& code);
std::string F1ShortGrandPrix(std::string name);
// Requests remain on the existing Jolpica host. A bounded lookup walks back
// through unpublished sessions; an empty result never erases cached results.
using F1Fetch = bool (*)(const char* url, std::string& body);
void RefreshFormulaOne(FormulaOne&, time_t now, std::string& body, F1Fetch fetch);
}
