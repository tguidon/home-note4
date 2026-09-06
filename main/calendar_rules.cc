#include "calendar_rules.h"
#include <algorithm>
#include <set>
#include <string_view>
namespace home::ical {
int Days(int y, int m, int d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yo = unsigned(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + int(yo * 365 + yo / 4 - yo / 100 + doy) - 719468;
}
Date Civil(int days) {
    days += 719468;
    const int era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = unsigned(days - era * 146097);
    const unsigned yo = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = int(yo) + era * 400;
    const unsigned doy = doe - (365 * yo + yo / 4 - yo / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const int d = int(doy - (153 * mp + 2) / 5 + 1);
    const int m = int(mp) + (mp < 10 ? 3 : -9);
    y += m <= 2;
    return {y, m, d, 0, 0, 0, true, false};
}
int MonthDays(int y, int m) {
    const int n[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return n[m-1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}
int Weekday(int days) { return ((days + 4) % 7 + 7) % 7; }
int64_t Wall(const Date& d) { return int64_t(Days(d.y,d.m,d.d))*86400 + d.h*3600 + d.min*60 + d.sec; }
namespace {
bool Number(std::string_view s, int& out, int low, int high) {
    if (s.empty() || s.size() > 8) return false;
    int sign = 1, n = 0;
    if (s[0] == '-' || s[0] == '+') { sign = s[0] == '-' ? -1 : 1; s.remove_prefix(1); }
    if (s.empty()) return false;
    for (char c : s) { if (c < '0' || c > '9') return false; n = n*10 + c-'0'; }
    out = n*sign; return out >= low && out <= high;
}
int DayName(std::string_view s) {
    const char* names[] = {"SU","MO","TU","WE","TH","FR","SA"};
    for (int i=0;i<7;++i) if (s == names[i]) return i;
    return -1;
}
bool List(std::string_view s, std::vector<int>& out, int low, int high, bool nonzero) {
    while (!s.empty()) {
        auto n = s.find(','); int v;
        if (!Number(s.substr(0,n),v,low,high) || (nonzero && !v) || out.size() >= 366) return false;
        out.push_back(v); if (n == s.npos) return true;
        s.remove_prefix(n+1); if (s.empty()) return false;
    }
    return false;
}
bool Has(const std::vector<int>& v, int n) { return std::find(v.begin(),v.end(),n) != v.end(); }
bool Matches(const Date& d, const Date& start, const Rule& r) {
    if (!r.months.empty() && !Has(r.months,d.m)) return false;
    if (!r.month_days.empty() && !Has(r.month_days,d.d) && !Has(r.month_days,d.d-MonthDays(d.y,d.m)-1)) return false;
    if (!r.weekdays.empty()) {
        bool found = false;
        const int day = Days(d.y,d.m,d.d), wd = Weekday(day);
        for (const auto& by : r.weekdays) {
            if (wd != by.weekday) continue;
            if (!by.ordinal) { found = true; break; }
            int first = Days(d.y,d.m,1), last = first + MonthDays(d.y,d.m)-1;
            if (r.frequency == "YEARLY" && r.months.empty()) { first=Days(d.y,1,1); last=Days(d.y,12,31); }
            if ((by.ordinal > 0 && (day-first)/7+1 == by.ordinal) ||
                (by.ordinal < 0 && -((last-day)/7+1) == by.ordinal)) found = true;
        }
        if (!found) return false;
    }
    if (r.frequency == "WEEKLY" && r.weekdays.empty() && Weekday(Days(d.y,d.m,d.d)) != Weekday(Days(start.y,start.m,start.d))) return false;
    if ((r.frequency == "MONTHLY" || r.frequency == "YEARLY") && r.weekdays.empty() && r.month_days.empty() && d.d != start.d) return false;
    if (r.frequency == "YEARLY" && r.months.empty() && r.weekdays.empty() && r.month_days.empty() && d.m != start.m) return false;
    return true;
}
}
bool ParseDate(const std::string& s, Date& d) {
    if (s.size()!=8 && s.size()!=15 && s.size()!=16) return false;
    auto digits = [&](size_t at,size_t n,int& v,int low,int high) {
        for (size_t i=at;i<at+n;++i) if (s[i]<'0'||s[i]>'9') return false;
        return Number(std::string_view(s).substr(at,n),v,low,high);
    };
    Date t;
    if (!digits(0,4,t.y,1900,2199) || !digits(4,2,t.m,1,12) || !digits(6,2,t.d,1,MonthDays(t.y,t.m))) return false;
    t.day = s.size()==8; t.utc = s.size()==16;
    if (!t.day && (s[8]!='T' || (t.utc && s[15]!='Z') || !digits(9,2,t.h,0,23) || !digits(11,2,t.min,0,59) || !digits(13,2,t.sec,0,59))) return false;
    d=t; return true;
}
bool ParseRule(const std::string& s, Rule& out) {
    if (s.empty() || s.size()>1024) return false;
    Rule r; std::set<std::string> seen;
    size_t at=0;
    while (at<s.size()) {
        const auto end=s.find(';',at), eq=s.find('=',at);
        if (eq==s.npos || (end!=s.npos && eq>end)) return false;
        const auto key=s.substr(at,eq-at), value=s.substr(eq+1,(end==s.npos?s.size():end)-eq-1);
        if (!seen.insert(key).second || value.empty()) return false;
        if (key=="FREQ") r.frequency=value;
        else if (key=="INTERVAL") { if (!Number(value,r.interval,1,10000)) return false; }
        else if (key=="COUNT") { if (!Number(value,r.count,1,1000000)) return false; }
        else if (key=="UNTIL") { if (!ParseDate(value,r.until)) return false; }
        else if (key=="WKST") { r.week_start=DayName(value); if (r.week_start<0) return false; }
        else if (key=="BYMONTH") { if (!List(value,r.months,1,12,false)) return false; }
        else if (key=="BYMONTHDAY") { if (!List(value,r.month_days,-31,31,true)) return false; }
        else if (key=="BYSETPOS") { if (!List(value,r.positions,-366,366,true)) return false; }
        else if (key=="BYDAY") {
            std::string_view v=value;
            while (!v.empty()) {
                const auto comma=v.find(','); const auto part=v.substr(0,comma);
                if (part.size()<2 || r.weekdays.size()>=366) return false;
                int ordinal=0, wd=DayName(part.substr(part.size()-2));
                if (wd<0 || (part.size()>2 && (!Number(part.substr(0,part.size()-2),ordinal,-53,53)||!ordinal))) return false;
                r.weekdays.push_back({ordinal,wd});
                if (comma==v.npos) break;
                v.remove_prefix(comma+1); if (v.empty()) return false;
            }
        } else return false; // Never silently approximate unsupported recurrence fields.
        if (end==s.npos) break;
        at=end+1; if (at==s.size()) return false;
    }
    if (r.frequency!="DAILY" && r.frequency!="WEEKLY" && r.frequency!="MONTHLY" && r.frequency!="YEARLY") return false;
    if (r.count && r.until.y) return false;
    if (r.frequency=="WEEKLY" && !r.month_days.empty()) return false;
    for (auto day:r.weekdays) if (day.ordinal && r.frequency!="MONTHLY" && r.frequency!="YEARLY") return false;
    if (!r.positions.empty() && r.months.empty() && r.month_days.empty() && r.weekdays.empty()) return false;
    out=std::move(r); return true;
}
bool Expand(const Date& start, const Rule& r, int first_day, int last_day, unsigned& budget, const std::function<bool(const Date&)>& emit, const std::function<bool(const Date&)>& valid_time) {
    const int origin=Days(start.y,start.m,start.d);
    int period=0, count=0;
    // COUNT needs the full history. Unbounded series can jump to the window.
    if (!r.count && first_day>origin) {
        const auto first=Civil(first_day);
        if (r.frequency=="DAILY") period=(first_day-origin)/r.interval;
        if (r.frequency=="WEEKLY") period=(first_day-origin)/(7*r.interval);
        if (r.frequency=="MONTHLY") period=((first.y-start.y)*12+first.m-start.m)/r.interval;
        if (r.frequency=="YEARLY") period=(first.y-start.y)/r.interval;
        period=std::max(0,period-1);
    }
    for (;;++period) {
        int begin=origin, end=origin+1;
        if (r.frequency=="DAILY") { begin=origin+period*r.interval; end=begin+1; }
        if (r.frequency=="WEEKLY") { begin=origin-(Weekday(origin)-r.week_start+7)%7+period*r.interval*7; end=begin+7; }
        if (r.frequency=="MONTHLY") {
            const int month=start.y*12+start.m-1+period*r.interval, y=month/12, m=month%12+1;
            if (y>2199) return true;
            begin=Days(y,m,1); end=begin+MonthDays(y,m);
        }
        if (r.frequency=="YEARLY") { const int y=start.y+period*r.interval; if (y>2199) return true; begin=Days(y,1,1); end=Days(y+1,1,1); }
        if (begin>last_day) return true;
        if (r.until.y && begin>Days(r.until.y,r.until.m,r.until.d)+1) return true;
        if (budget==0) return false;
        --budget;
        std::vector<int> candidates;
        for (int n=begin;n<end;++n) {
            if (budget==0) return false;
            --budget;
            if (Matches(Civil(n),start,r)) candidates.push_back(n);
        }
        if (!r.positions.empty()) {
            std::vector<int> selected;
            for (int pos:r.positions) { const int i=pos>0?pos-1:int(candidates.size())+pos;
                if (i>=0 && i<int(candidates.size())) selected.push_back(candidates[i]); }
            std::sort(selected.begin(),selected.end());
            selected.erase(std::unique(selected.begin(),selected.end()),selected.end()); candidates=std::move(selected);
        }
        for (int n:candidates) {
            if (n<origin) continue;
            Date d=Civil(n); d.h=start.h;d.min=start.min;d.sec=start.sec;d.day=start.day;d.utc=start.utc;
            if (r.until.y && !r.until.utc && Wall(d)>Wall(r.until)) return true;
            if (valid_time && !valid_time(d)) continue;
            if (r.count && ++count>r.count) return true;
            if (n>=first_day && n<=last_day && !emit(d)) return true;
        }
    }
}
}
