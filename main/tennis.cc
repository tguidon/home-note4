#include "tennis.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>
#include "cJSON.h"
#define JSMN_STATIC
#define JSMN_STRICT
#define JSMN_PARENT_LINKS
#include "third_party/jsmn.h"

namespace home {
namespace {
// ESPN returns the entire tournament even with a date filter. Tokenize once,
// then build a JSON tree for only one match at a time, instead of a multi-MB DOM.
class Document {
public:
    explicit Document(const std::string& source) : source_(source) {}
    bool Parse() {
        jsmn_parser parser; jsmn_init(&parser);
        count_ = jsmn_parse(&parser, source_.data(), source_.size(), nullptr, 0);
        if (count_ < 1 || count_ > 200000) return false;
        tokens_.reset(static_cast<jsmntok_t*>(malloc(count_ * sizeof(jsmntok_t))));
        if (!tokens_) return false;
        jsmn_init(&parser);
        return jsmn_parse(&parser, source_.data(), source_.size(), tokens_.get(), count_) == count_ &&
               tokens_.get()[0].type == JSMN_OBJECT && Next(0) == count_;
    }
    int Next(int i) const {
        if (i < 0 || i >= count_) return count_;
        const int end = tokens_.get()[i++].end;
        while (i < count_ && tokens_.get()[i].start < end) ++i;
        return i;
    }
    bool Is(int i, jsmntype_t type) const { return i >= 0 && i < count_ && tokens_.get()[i].type == type; }
    std::string_view Text(int i) const {
        if (i < 0 || i >= count_) return {};
        const auto& t = tokens_.get()[i]; return {source_.data() + t.start, static_cast<size_t>(t.end - t.start)};
    }
    int Field(int object, std::string_view key) const {
        if (!Is(object, JSMN_OBJECT)) return -1;
        const int end = tokens_.get()[object].end;
        for (int i = object + 1; i < count_ && tokens_.get()[i].start < end;) {
            if (!Is(i, JSMN_STRING) || i + 1 >= count_) return -1;
            if (Text(i) == key) return i + 1;
            i = Next(i + 1);
        }
        return -1;
    }
    int End(int i) const { return Next(i); }
private:
    const std::string& source_;
    std::unique_ptr<jsmntok_t, decltype(&free)> tokens_{nullptr, free};
    int count_ = 0;
};
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
const cJSON* Get(const cJSON* p, const char* k) { return cJSON_GetObjectItemCaseSensitive(p, k); }
std::string Str(const cJSON* p, const char* key) {
    auto* v = Get(p, key); return cJSON_IsString(v) ? v->valuestring : "";
}
int Priority(const TennisMatch& m) { return m.state == "in" ? 0 : m.state == "pre" ? 1 : 2; }
bool ReadMatch(std::string_view text, TennisMatch& out) {
    Json j(cJSON_ParseWithLength(text.data(), text.size()), cJSON_Delete);
    if (!j) return false;
    TennisMatch m;
    m.id = Str(j.get(), "id"); m.date = Str(j.get(), "date");
    m.time_valid = cJSON_IsTrue(Get(j.get(), "timeValid"));
    const auto* status = Get(Get(j.get(), "status"), "type");
    m.state = Str(status, "state"); m.detail = Str(status, "shortDetail");
    m.completed = cJSON_IsTrue(Get(status, "completed"));
    m.round = Str(Get(j.get(), "round"), "displayName");
    m.court = Str(Get(j.get(), "venue"), "court");
    m.division = Str(Get(j.get(), "type"), "text");
    const auto* competitors = Get(j.get(), "competitors");
    if (m.id.empty() || !ParseUtcTime(m.date) || m.state.empty() || !cJSON_IsArray(competitors) || cJSON_GetArraySize(competitors) != 2) return false;
    for (int i = 0; i < 2; ++i) {
        const auto* player = cJSON_GetArrayItem(competitors, i);
        m.players[i] = Str(Get(player, "athlete"), "shortName");
        if (m.players[i].empty()) m.players[i] = Str(Get(player, "roster"), "shortDisplayName");
        if (m.players[i].empty()) m.players[i] = "TBD";
        m.winners[i] = cJSON_IsTrue(Get(player, "winner"));
        const cJSON* score;
        cJSON_ArrayForEach(score, Get(player, "linescores")) {
            auto* v = Get(score, "value");
            if (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) || v->valuedouble < 0 || v->valuedouble > 999) return false;
            if (!m.scores[i].empty()) m.scores[i] += " ";
            m.scores[i] += std::to_string(v->valueint);
            if (m.scores[i].size() > 30) break;
        }
    }
    out = std::move(m); return true;
}
}
int LocalDateKey(time_t now) {
    tm local = {}; localtime_r(&now, &local);
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}
bool ParseTennis(const std::string& json, time_t now, Tennis& out, const char** error) {
    auto fail = [&](const char* reason) { if (error) *error = reason; return false; };
    if (error) *error = "none";
    Document doc(json); if (!doc.Parse()) return fail("JSON tokenization or allocation failed");
    int events = doc.Field(0, "events"); if (!doc.Is(events, JSMN_ARRAY)) return fail("events missing");
    Tennis result; result.day = LocalDateKey(now);
    const int events_end = doc.End(events);
    for (int e = events + 1; e < events_end; e = doc.Next(e)) {
        if (doc.Text(doc.Field(e, "name")) != "US Open") continue;
        result.event_found = true;
        result.starts = ParseUtcTime(std::string(doc.Text(doc.Field(e, "date"))));
        result.ends = ParseUtcTime(std::string(doc.Text(doc.Field(e, "endDate"))));
        if (!result.starts || result.ends <= result.starts) return fail("invalid tournament dates");
        int groups = doc.Field(e, "groupings"); if (!doc.Is(groups, JSMN_ARRAY)) return fail("groupings missing");
        const int groups_end = doc.End(groups);
        for (int g = groups + 1; g < groups_end; g = doc.Next(g)) {
            int matches = doc.Field(g, "competitions"); if (!doc.Is(matches, JSMN_ARRAY)) return fail("competitions missing");
            const int matches_end = doc.End(matches);
            for (int m = matches + 1; m < matches_end; m = doc.Next(m)) {
                const auto date = ParseUtcTime(std::string(doc.Text(doc.Field(m, "date"))));
                const int status = doc.Field(doc.Field(m, "status"), "type");
                const bool live = doc.Text(doc.Field(status, "state")) == "in";
                if (!date) return fail("invalid match date");
                if (LocalDateKey(date) != result.day && !live) continue;
                TennisMatch match;
                if (!ReadMatch(doc.Text(m), match)) return fail("invalid selected match");
                if (result.matches.size() >= 160) return fail("match limit exceeded");
                result.matches.push_back(std::move(match));
            }
        }
    }
    std::stable_sort(result.matches.begin(), result.matches.end(), [](const auto& a, const auto& b) {
        if (Priority(a) != Priority(b)) return Priority(a) < Priority(b);
        return a.state == "post" ? a.date > b.date : a.date < b.date;
    });
    // Retain discovered event dates when the date-filtered endpoint moves on.
    if (!result.event_found) { result.starts = out.starts; result.ends = out.ends; }
    result.valid = true; out = std::move(result); return true;
}
bool TennisEnded(const Tennis& tennis, time_t now) {
    // Published 2026 endpoint endDate, also usable after an off-season reboot.
    const auto known_end = ParseUtcTime("2026-09-14T03:59Z");
    return (tennis.ends && now > tennis.ends) ||
        (!tennis.ends && LocalDateKey(now) / 10000 == 2026 && now > known_end);
}
std::string TennisEmptyTitle(const Tennis& tennis, time_t now) {
    if (TennisEnded(tennis, now)) return "US Open is over";
    if (!tennis.valid || tennis.day != LocalDateKey(now)) return "Loading US Open";
    if (tennis.starts && now < tennis.starts) return "US Open starts soon";
    return tennis.event_found ? "No matches scheduled today" : "US Open is not in session";
}
}
