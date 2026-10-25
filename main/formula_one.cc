#include "formula_one.h"
#include "time_utils.h"
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

namespace home {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
const cJSON* Get(const cJSON* j, const char* k) { return cJSON_GetObjectItemCaseSensitive(j, k); }
std::string Str(const cJSON* j, const char* k) {
    const auto* v = Get(j, k);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}
bool Supported(const std::string& c) {
    return c == "FP1" || c == "FP2" || c == "FP3" || c == "SQ" || c == "SR" || c == "Q" || c == "R";
}
bool SafeId(const std::string& id) {
    return !id.empty() && id.size() <= 80 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos;
}
bool Cancelled(const cJSON* s) {
    if (cJSON_IsTrue(Get(s, "is_cancelled"))) return true;
    const auto* sessions = Get(s, "sessions");
    if (!cJSON_IsArray(sessions) || cJSON_GetArraySize(sessions) == 0) return false;
    const cJSON* item;
    cJSON_ArrayForEach(item, sessions) if (!cJSON_IsTrue(Get(item, "is_cancelled"))) return false;
    return true;
}
void Sort(std::vector<F1Session>& sessions) {
    std::sort(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.starts < b.starts; });
}
bool FetchSeason(int year, std::string& body, F1Fetch fetch, std::vector<F1Session>& sessions) {
    const std::string url = "https://api.jolpi.ca/f1/alpha/schedules/" + std::to_string(year) + "/";
    return fetch(url.c_str(), body) && ParseF1Schedule(body, year, sessions);
}
}
bool ParseF1Schedule(const std::string& body, int year, std::vector<F1Session>& out) {
    Json j(cJSON_Parse(body.c_str()), cJSON_Delete);
    const auto* data = Get(j.get(), "data");
    const auto* y = Get(data, "year");
    const auto* events = Get(data, "events");
    if (!cJSON_IsNumber(y) || y->valuedouble != year || !cJSON_IsArray(events) || cJSON_GetArraySize(events) > 40) return false;
    std::vector<F1Session> parsed;
    std::set<std::string> keys;
    const cJSON* event;
    cJSON_ArrayForEach(event, events) {
        const auto* round = Get(event, "round");
        if (cJSON_IsTrue(Get(round, "is_cancelled"))) continue;
        const auto id = Str(round, "id"), name = Str(round, "name");
        const auto* schedule = Get(event, "schedule");
        if (!SafeId(id) || name.empty() || name.size() > 160 || !cJSON_IsArray(schedule)) return false;
        const cJSON* item;
        cJSON_ArrayForEach(item, schedule) {
            const auto code = Str(item, "code");
            if (!Supported(code) || Cancelled(item)) continue;
            F1Session s{id, name, code, Str(item, "timestamp"), 0};
            s.starts = ParseUtcTime(s.timestamp);
            // Sessions with a TBA start cannot drive a date-based anchor.
            if (!s.starts && s.timestamp.empty()) continue;
            if (!s.starts || !keys.insert(id + "/" + code).second || parsed.size() >= 280) return false;
            parsed.push_back(std::move(s));
        }
    }
    Sort(parsed); out = std::move(parsed); return true;
}
F1ResultState ParseF1Result(const std::string& body, const F1Session& expected, F1Result& out) {
    Json j(cJSON_Parse(body.c_str()), cJSON_Delete);
    const auto* data = Get(j.get(), "data");
    const auto* round = Get(data, "round");
    const auto* rows = Get(data, "results");
    if (!Supported(expected.code) || Str(round, "id") != expected.round_id || Str(data, "code") != expected.code ||
        ParseUtcTime(Str(data, "timestamp")) != expected.starts || !cJSON_IsArray(rows) || cJSON_GetArraySize(rows) > 30) return F1ResultState::Invalid;
    if (Cancelled(data) || cJSON_IsTrue(Get(round, "is_cancelled")) || cJSON_GetArraySize(rows) == 0) return F1ResultState::Pending;
    F1Result result; result.session = expected;
    std::set<std::string> drivers;
    std::set<int> positions;
    const cJSON* row;
    cJSON_ArrayForEach(row, rows) {
        const auto* driver = Get(row, "driver");
        const auto id = Str(driver, "id"), family = Str(driver, "family_name"), given = Str(driver, "given_name");
        const auto* pos = Get(row, "position");
        if (id.empty() || family.empty() || family.size() > 80 || !drivers.insert(id).second ||
            !cJSON_IsNumber(pos) || !std::isfinite(pos->valuedouble) || pos->valuedouble < 1 ||
            pos->valuedouble > 30 || pos->valuedouble != pos->valueint || !positions.insert(pos->valueint).second) return F1ResultState::Invalid;
        F1Row r; r.position = pos->valueint; r.position_text = Str(row, "position_text");
        if (r.position_text.empty()) r.position_text = std::to_string(r.position);
        r.driver = (given.empty() ? "" : given.substr(0, 1) + ". ") + family;
        r.time = Str(row, "time");
        const auto status = Str(row, "status");
        if (status == "RETIRED") r.time = "DNF";
        else if (status == "DISQUALIFIED") r.time = "DSQ";
        else if (status == "DID NOT START" || status == "DID_NOT_START") r.time = "DNS";
        if (r.time.empty()) r.time = status.empty() ? "No time" : status;
        if (r.position_text.size() > 8 || r.time.size() > 80) return F1ResultState::Invalid;
        result.rows.push_back(std::move(r));
    }
    std::sort(result.rows.begin(), result.rows.end(), [](const auto& a, const auto& b) { return a.position < b.position; });
    // This API publishes classifications, not a live-timing stream, and has
    // no finality flag. Label these as published results, never official grids.
    out = std::move(result); return F1ResultState::Published;
}
const F1Session* F1Anchor(const FormulaOne& f, time_t now) {
    if (now < 1767225600) return nullptr;
    const int today = LocalDateKey(now);
    for (const auto& s : f.sessions) if (s.code == "R" && LocalDateKey(s.starts) >= today) return &s;
    return nullptr;
}
const char* F1SessionTitle(const std::string& code) {
    if (code == "FP1") return "Practice 1";
    if (code == "FP2") return "Practice 2";
    if (code == "FP3") return "Practice 3";
    if (code == "SQ") return "Sprint qualifying";
    if (code == "SR") return "Sprint race";
    if (code == "Q") return "Qualifying order";
    return "Race results";
}
std::string F1ShortGrandPrix(std::string name) {
    const auto at = name.find("Grand Prix");
    if (at != std::string::npos) name.replace(at, 10, "GP");
    return name;
}
void RefreshFormulaOne(FormulaOne& f, time_t now, std::string& body, F1Fetch fetch) {
    if (now < 1767225600) return;
    tm local{}; localtime_r(&now, &local); const int year = local.tm_year + 1900;
    f.error = false;
    bool session_today = std::any_of(f.sessions.begin(), f.sessions.end(), [now](const auto& s) {
        return LocalDateKey(s.starts) == LocalDateKey(now);
    });
    if (!f.schedule_valid || f.year != year || LocalDateKey(now) != LocalDateKey(f.schedule_updated) ||
        now - f.schedule_updated >= (session_today ? 3600 : 86400)) {
        std::vector<F1Session> sessions;
        if (FetchSeason(year, body, fetch, sessions)) {
            // Cold starts before the first session can still show last year's
            // last race; after the finale, look for the announced next season.
            if (std::none_of(sessions.begin(), sessions.end(), [now](const auto& s) { return s.starts <= now; })) {
                std::vector<F1Session> previous;
                if (FetchSeason(year - 1, body, fetch, previous)) sessions.insert(sessions.end(), previous.begin(), previous.end());
            }
            f.sessions = std::move(sessions); Sort(f.sessions);
            if (!F1Anchor(f, now)) {
                std::vector<F1Session> next;
                if (FetchSeason(year + 1, body, fetch, next)) f.sessions.insert(f.sessions.end(), next.begin(), next.end());
                Sort(f.sessions);
            }
            f.schedule_valid = true; f.year = year; f.schedule_updated = now;
        } else f.error = true;
    }
    f.pending.clear();
    int attempts = 0;
    for (auto it = f.sessions.rbegin(); it != f.sessions.rend(); ++it) {
        if (it->starts > now) continue;
        if (it->starts < f.latest.session.starts || attempts >= 8) break;
        ++attempts;
        const std::string url = "https://api.jolpi.ca/f1/alpha/results/" + it->round_id + "/" + it->code + "/";
        F1Result result;
        const auto state = fetch(url.c_str(), body) ? ParseF1Result(body, *it, result) : F1ResultState::Invalid;
        if (state == F1ResultState::Published) {
            if (f.latest.session.round_id != result.session.round_id || f.latest.session.code != result.session.code)
                f.classification_seen = now;
            f.latest = std::move(result); f.updated = now; break;
        }
        if (state == F1ResultState::Invalid) f.error = true;
        else f.updated = now;
        if (f.pending.empty() && it->starts > f.latest.session.starts) f.pending = it->code;
    }
    if (!attempts && f.schedule_valid && !f.error) f.updated = now;
}
}
