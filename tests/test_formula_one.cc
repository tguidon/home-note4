#include "dashboard.h"
#include "render.h"
#include "cJSON.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

using namespace home;
std::string Load(const std::string& name) {
    std::ifstream f("tests/fixtures/f1-" + name + ".json"); assert(f.good());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
std::string Modify(const std::string& name, void (*edit)(cJSON*)) {
    auto* j = cJSON_Parse(Load(name).c_str()); assert(j);
    edit(cJSON_GetObjectItemCaseSensitive(j, "data"));
    char* raw = cJSON_PrintUnformatted(j); std::string s(raw);
    cJSON_free(raw); cJSON_Delete(j); return s;
}
const F1Session& Session(const FormulaOne& f, const std::string& code, const std::string& round = "round_rcsVMlOc") {
    for (const auto& s : f.sessions) if (s.code == code && s.round_id == round) return s;
    assert(false); return f.sessions.front();
}
std::map<std::string, std::string> responses;
int requests = 0;
bool Fetch(const char* url, std::string& body) {
    ++requests;
    assert(std::string(url).find("https://api.jolpi.ca/f1/alpha/") == 0);
    const auto it = responses.find(url); if (it == responses.end()) return false;
    body = it->second; return true;
}
std::string Url(const F1Session& s) { return "https://api.jolpi.ca/f1/alpha/results/" + s.round_id + "/" + s.code + "/"; }
void Preview(const FormulaOne& f, time_t now, const std::string& file, int detail = 0) {
    Dashboard d; d.f1 = f; ZectrixCanvas c;
    RenderDashboard(c, d, SectionStart(d, 1) + detail, now, true, 87);
    std::ofstream out(file, std::ios::binary); out << "P4\n400 300\n";
    for (size_t i = 0; i < c.size(); ++i) out.put(static_cast<char>(~c.data()[i]));
}
int main(int argc, char** argv) {
    setenv("TZ", kTimezone, 1); tzset();
    FormulaOne f;
    assert(ParseF1Schedule(Load("schedule"), 2026, f.sessions));
    f.schedule_valid = true; f.year = 2026;
    const auto now = ParseUtcTime("2026-09-06T00:30Z"); // Still Saturday in New York.
    const auto sunday = ParseUtcTime("2026-09-06T20:00Z");
    const auto monday = ParseUtcTime("2026-09-07T04:00Z");
    assert(F1Anchor(f, now)->grand_prix == "Italian Grand Prix");
    assert(F1Anchor(f, sunday)->grand_prix == "Italian Grand Prix");
    assert(F1Anchor(f, monday - 1)->grand_prix == "Italian Grand Prix");
    assert(F1Anchor(f, monday)->grand_prix != "Italian Grand Prix");
    const auto saved_size = f.sessions.size();
    assert(!ParseF1Schedule("{}", 2026, f.sessions) && f.sessions.size() == saved_size);
    assert(!ParseF1Schedule(Load("schedule"), 2027, f.sessions));
    assert(!ParseF1Schedule(Modify("schedule", [](cJSON* d) {
        auto* event = cJSON_GetArrayItem(cJSON_GetObjectItem(d, "events"), 0);
        auto* round = cJSON_GetObjectItem(event, "round");
        cJSON_ReplaceItemInObject(round, "id", cJSON_CreateString("../../bad"));
    }), 2026, f.sessions));
    std::vector<F1Session> cancelled;
    assert(ParseF1Schedule(Modify("schedule", [](cJSON* d) {
        auto* event = cJSON_GetArrayItem(cJSON_GetObjectItem(d, "events"), 0);
        auto* round = cJSON_GetObjectItem(event, "round");
        cJSON_ReplaceItemInObject(round, "is_cancelled", cJSON_CreateTrue());
    }), 2026, cancelled));
    assert(cancelled.size() == saved_size - 5);
    for (const auto& code : {"FP1", "FP2", "FP3", "Q", "SQ", "SR", "R"}) {
        const std::string c = code;
        const auto& s = Session(f, c, c == "SQ" || c == "SR" ? "round_JqmngOvt" : c == "R" ? "round_OlKoikhn" : "round_rcsVMlOc");
        F1Result result;
        assert(ParseF1Result(Load(c == "R" ? "past-R" : c), s, result) == F1ResultState::Published);
        assert(result.rows.size() >= 21 && result.rows.front().position == 1);
        assert(!result.rows.front().time.empty());
        for (size_t i = 1; i < result.rows.size(); ++i) assert(result.rows[i - 1].position < result.rows[i].position);
        if (c == "SR" || c == "R") assert(result.rows.back().time == "DNF");
        if (c == "Q") { assert(result.rows.front().driver == "P. Gasly"); f.latest = result; }
        if (argc > 1) { auto preview = f; preview.latest = result; preview.updated = now; Preview(preview, now, std::string(argv[1]) + "/f1-" + c + ".pbm"); }
    }
    const auto q = f.latest;
    assert(ParseF1Result(Load("R"), Session(f, "R"), f.latest) == F1ResultState::Pending);
    assert(f.latest.session.code == "Q" && f.latest.rows.front().driver == q.rows.front().driver);
    assert(ParseF1Result(Load("Q"), Session(f, "FP3"), f.latest) == F1ResultState::Invalid);
    assert(ParseF1Result("{", Session(f, "Q"), f.latest) == F1ResultState::Invalid);
    assert(ParseF1Result(Modify("Q", [](cJSON* d) {
        auto* row = cJSON_GetArrayItem(cJSON_GetObjectItem(d, "results"), 0);
        cJSON_ReplaceItemInObject(row, "position", cJSON_CreateNumber(1.5));
    }), Session(f, "Q"), f.latest) == F1ResultState::Invalid);
    assert(f.latest.rows.size() == q.rows.size());
    Dashboard d; d.f1 = f;
    assert(SectionPages(d, 1) == 6 && SectionStart(d, 2) == SectionStart(d, 1) + 6);
    assert(PageCount(d) == 8);
    // Deterministic fake HTTP: a cold boot finds the newest published session.
    responses["https://api.jolpi.ca/f1/alpha/schedules/2026/"] = Load("schedule");
    for (const auto& c : {"FP1", "FP2", "FP3", "Q", "R"}) responses[Url(Session(f, c))] = Load(c);
    responses[Url(Session(f, "SQ", "round_JqmngOvt"))] = Load("SQ");
    responses[Url(Session(f, "SR", "round_JqmngOvt"))] = Load("SR");
    std::string body; FormulaOne live;
    RefreshFormulaOne(live, now, body, Fetch);
    assert(!live.error && live.latest.session.code == "Q" && requests == 2);
    const auto stamp = live.updated;
    requests = 0;
    RefreshFormulaOne(live, now + 300, body, Fetch);
    assert(requests == 1 && live.updated > stamp); // Schedule cached for an hour.
    responses[Url(Session(f, "R"))] = Load("R");
    RefreshFormulaOne(live, sunday, body, Fetch);
    assert(!live.error && live.pending == "R" && live.latest.session.code == "Q");
    assert(F1Anchor(live, sunday)->round_id == "round_rcsVMlOc");
    // Simulated Monza race classification tests clock behavior, not predictions.
    const auto finished = Modify("past-R", [](cJSON* data) {
        auto* round = cJSON_GetObjectItem(data, "round");
        cJSON_ReplaceItemInObject(round, "id", cJSON_CreateString("round_rcsVMlOc"));
        cJSON_ReplaceItemInObject(data, "timestamp", cJSON_CreateString("2026-09-06T13:00:00Z"));
    });
    responses[Url(Session(f, "R"))] = finished;
    RefreshFormulaOne(live, sunday + 300, body, Fetch);
    assert(!live.error && live.pending.empty() && live.latest.session.code == "R");
    assert(F1Anchor(live, monday - 1)->round_id == live.latest.session.round_id);
    assert(F1Anchor(live, monday)->round_id != live.latest.session.round_id);
    FormulaOne cold_monday;
    RefreshFormulaOne(cold_monday, monday, body, Fetch);
    assert(cold_monday.latest.session.code == "R" && F1Anchor(cold_monday, monday)->round_id != cold_monday.latest.session.round_id);
    if (argc > 1) {
        const std::string dir = argv[1];
        Preview(live, sunday, dir + "/f1-race-day.pbm");
        Preview(live, monday, dir + "/f1-next-day.pbm");
        Preview(live, monday, dir + "/f1-last-page.pbm", 5);
    }
    // Missing newer results fall back on a cold start; never erase prior data.
    responses[Url(Session(f, "Q"))] = Modify("Q", [](cJSON* data) { cJSON_ReplaceItemInObject(data, "results", cJSON_CreateArray()); });
    FormulaOne pending;
    RefreshFormulaOne(pending, now, body, Fetch);
    assert(!pending.error && pending.pending == "Q" && pending.latest.session.code == "FP3");
    if (argc > 1) Preview(pending, now, std::string(argv[1]) + "/f1-pending.pbm");
    responses.clear(); const auto saved = live.latest.rows.front().driver;
    RefreshFormulaOne(live, monday + 3600, body, Fetch);
    assert(live.error && live.latest.rows.front().driver == saved);
    if (argc > 1) Preview(live, monday, std::string(argv[1]) + "/f1-offline.pbm");
    // Latest practice and sprint sessions are selected by timestamp, not a
    // hard-coded weekend shape or the API's input order.
    responses["https://api.jolpi.ca/f1/alpha/schedules/2026/"] = Load("schedule");
    for (const auto& code : {"FP1", "FP2", "FP3", "SQ", "SR"}) {
        const std::string c = code;
        const auto& s = Session(f, c, c == "SQ" || c == "SR" ? "round_JqmngOvt" : "round_rcsVMlOc");
        responses[Url(s)] = Load(c);
        FormulaOne weekend;
        RefreshFormulaOne(weekend, s.starts + 3600, body, Fetch);
        assert(!weekend.error && weekend.latest.session.code == c);
    }
    FormulaOne ended = f;
    assert(!F1Anchor(ended, ParseUtcTime("2026-12-31T12:00Z")));
    assert(ParseF1Schedule(R"({"data":{"year":2027,"events":[]}})", 2027, cancelled) && cancelled.empty());
    std::cout << "PASS: all seven F1 session types, race-day/midnight anchors, full classification, pending/failure fallback, cold starts, schedule cache, and cancellation\n";
}
