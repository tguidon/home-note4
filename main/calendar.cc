#include "calendar.h"
#include "calendar_rules.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <string_view>
#include <utility>
namespace home {
namespace {
using namespace ical;
constexpr int kHorizon = 90;
constexpr size_t kMaxEvents = 2000, kMaxOccurrences = 1024;
struct Property { std::string value, zone, type, range; };
using Fields = std::map<std::string, std::vector<Property>>;
struct Zone { std::string name; std::vector<Fields> shifts; };
struct Document { std::vector<Fields> events; std::map<std::string,Zone> zones; };
std::string Upper(std::string s) { for (char& c:s) c=char(std::toupper(static_cast<unsigned char>(c))); return s; }
const Property& Get(const Fields& f, const char* key) {
    static const Property empty;
    auto it=f.find(key); return it==f.end() || it->second.empty()?empty:it->second.front();
}
std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i=0;i<s.size() && out.size()<160;++i) {
        char c=s[i]; if (c=='\\' && i+1<s.size()) { c=s[++i]; if (c=='n'||c=='N') c=' '; }
        if (static_cast<unsigned char>(c)>=32 && c!=127) out+=c;
        else out+=' ';
    }
    return out.empty()?"Untitled event":out;
}
bool ReadProperty(const std::string& line, std::string& key, Property& p) {
    bool quoted=false; size_t colon=std::string::npos;
    for (size_t i=0;i<line.size();++i) { if (line[i]=='"') quoted=!quoted; if (line[i]==':' && !quoted) { colon=i;break; } }
    if (colon==line.npos || quoted) return false;
    const auto semi=line.find(';'); key=Upper(line.substr(0,std::min(colon,semi)));
    p.value=line.substr(colon+1);
    size_t at=semi;
    while (at<colon) {
        size_t end=at+1; quoted=false;
        while (end<colon) { if (line[end]=='"') quoted=!quoted; if(line[end]==';'&&!quoted) break; ++end; }
        const auto eq=line.find('=',at+1); if (eq==line.npos||eq>=end) return false;
        auto name=Upper(line.substr(at+1,eq-at-1)), value=line.substr(eq+1,end-eq-1);
        if (value.size()>=2 && value.front()=='"' && value.back()=='"') value=value.substr(1,value.size()-2);
        if (name=="TZID") p.zone=value;
        if (name=="VALUE") p.type=Upper(value);
        if (name=="RANGE") p.range=Upper(value);
        at=end;
    }
    return !key.empty();
}
bool ReadDocument(const std::string& body, Document& doc) {
    if (body.size()>3*1024*1024 || body.find('\0')!=body.npos) return false;
    std::vector<std::string> stack;
    Fields event, shift; Zone zone;
    bool complete=false, started=false;
    auto line = [&](const std::string& s) {
        if (s.empty()) return true;
        if (complete) return false;
        std::string key; Property p;
        if (!ReadProperty(s,key,p)) return false;
        if (key=="BEGIN") {
            auto type=Upper(p.value);
            if (stack.empty()) { if(started||type!="VCALENDAR") return false; started=true; }
            else if ((type=="VEVENT" || type=="VTIMEZONE") && stack.back()!="VCALENDAR") return false;
            if (stack.size()>8) return false;
            stack.push_back(type);
            if(type=="VEVENT") event.clear();
            if(type=="VTIMEZONE") zone={};
            if(type=="STANDARD"||type=="DAYLIGHT") { if(stack.size()!=3||stack[1]!="VTIMEZONE") return false; shift.clear(); }
            return true;
        }
        if (key=="END") {
            if(stack.empty()||stack.back()!=Upper(p.value)) return false;
            const auto type=stack.back(); stack.pop_back();
            if(type=="VEVENT") { if(doc.events.size()>=kMaxEvents) return false; doc.events.push_back(std::move(event)); }
            if(type=="STANDARD"||type=="DAYLIGHT") { if(zone.shifts.size()>=32) return false; zone.shifts.push_back(std::move(shift)); }
            if(type=="VTIMEZONE") { if(zone.name.empty()||doc.zones.size()>=32||doc.zones.count(zone.name)) return false; doc.zones.emplace(zone.name,std::move(zone)); }
            if(type=="VCALENDAR") complete=true;
            return true;
        }
        if(stack.empty()) return false;
        if(stack.back()=="VTIMEZONE"&&key=="TZID") zone.name=p.value;
        if(stack.back()=="VCALENDAR" && key=="METHOD" && Upper(p.value)=="CANCEL") return false;
        static const std::set<std::string> keep={"UID","DTSTART","DTEND","DURATION","SUMMARY","STATUS","RECURRENCE-ID","RRULE","RDATE","EXDATE","EXRULE","TZOFFSETFROM","TZOFFSETTO"};
        Fields* fields=stack.back()=="VEVENT"?&event:(stack.back()=="STANDARD"||stack.back()=="DAYLIGHT")?&shift:nullptr;
        if(fields&&keep.count(key)) {
            auto& values=(*fields)[key];
            if (p.value.size()>2048 || values.size()>=128) return false;
            if (!values.empty()&&key!="RDATE"&&key!="EXDATE") return false;
            values.push_back(std::move(p));
        }
        return true;
    };
    std::string logical;
    size_t at=0;
    while(at<body.size()) {
        auto end=body.find('\n',at); if(end==body.npos) end=body.size();
        size_t len=end-at; if(len&&body[at+len-1]=='\r') --len;
        bool folded=len&&(body[at]==' '||body[at]=='\t');
        if(!folded) { if(!line(logical)) return false; logical.clear(); }
        else { if(logical.empty()) return false; ++at; --len; }
        if(logical.size()+len>32768) return false;
        logical.append(body,at,len); at=end+1;
    }
    return line(logical)&&started&&complete&&stack.empty();
}
bool Offset(const std::string& s, int& seconds) {
    if((s.size()!=5&&s.size()!=7)||(s[0]!='+'&&s[0]!='-')) return false;
    for(size_t i=1;i<s.size();++i) if(s[i]<'0'||s[i]>'9') return false;
    const int h=(s[1]-'0')*10+s[2]-'0',m=(s[3]-'0')*10+s[4]-'0';
    const int sec=s.size()==7?(s[5]-'0')*10+s[6]-'0':0;
    if(h>23||m>59||sec>59) return false;
    seconds=(h*3600+m*60+sec)*(s[0]=='-'?-1:1); return true;
}
bool Dates(const Fields& f, const char* key, const std::function<bool(Property)>& emit) {
    auto it=f.find(key); if(it==f.end()) return true;
    for (const auto& p:it->second) {
        size_t at=0;
        do {
            const auto comma=p.value.find(',',at); Property single=p;
            single.value=p.value.substr(at,(comma==p.value.npos?p.value.size():comma)-at);
            if(!emit(single)) return false;
            if(comma==p.value.npos) break;
            at=comma+1;
        } while(at<=p.value.size());
    }
    return true;
}
class Times {
public:
    Times(const Document& doc,unsigned& budget):doc_(doc),budget_(budget) {}
    bool Convert(const Property& p, Date& d, time_t& stamp, bool* gap=nullptr) {
        if(!ParseDate(p.value,d)||(p.type!=""&&p.type!=(d.day?"DATE":"DATE-TIME"))||(!p.zone.empty()&&(d.day||d.utc))) return false;
        return Convert(d,p.zone,stamp,gap);
    }
    bool Convert(const Date& d,const std::string& zone,time_t& stamp,bool* gap=nullptr) {
        if(gap) *gap=false;
        if(d.utc) { stamp=Wall(d); return true; }
        if(d.day || ((zone.empty()||zone=="America/New_York"||zone=="US/Eastern") && d.y>=2007)) {
            const int march=Days(d.y,3,1), november=Days(d.y,11,1);
            const int spring=march+(7-Weekday(march))%7+7;
            const int fall=november+(7-Weekday(november))%7;
            const auto wall=Wall(d);
            const auto spring_wall=int64_t(spring)*86400+2*3600;
            const auto fall_wall=int64_t(fall)*86400+2*3600;
            if(gap) *gap=wall>=spring_wall && wall<spring_wall+3600;
            const bool daylight=wall>=spring_wall+3600 && wall<fall_wall;
            stamp=wall+(daylight?4:5)*3600;
            return true;
        }
        if(zone.empty() || ((zone=="America/New_York"||zone=="US/Eastern") && !doc_.zones.count(zone))) {
            tm local{}; local.tm_year=d.y-1900;local.tm_mon=d.m-1;local.tm_mday=d.d;
            local.tm_hour=d.h;local.tm_min=d.min;local.tm_sec=d.sec;local.tm_isdst=-1;
            stamp=mktime(&local);
            if(gap) *gap=local.tm_hour!=d.h||local.tm_min!=d.min;
            return stamp!=-1;
        }
        if(zone=="UTC"||zone=="Etc/UTC"||zone=="GMT"||zone=="Etc/GMT") { stamp=Wall(d);return true; }
        auto found=doc_.zones.find(zone); if(found==doc_.zones.end()) return false;
        const auto cache_key=std::make_pair(zone,d.y);
        auto cached=transitions_.find(cache_key);
        if(cached==transitions_.end()) {
            std::vector<Transition> shifts;
            if(!Build(found->second,d.y,shifts)) return false;
            cached=transitions_.emplace(cache_key,std::move(shifts)).first;
        }
        const auto& shifts=cached->second;
        if(shifts.empty()) return false;
        int offset=shifts.front().from;
        const auto wall=Wall(d);
        for(const auto& t:shifts) {
            // Fold: first occurrence; gap: pre-transition offset (RFC 5545).
            if(wall>=t.wall && wall<t.wall+std::max(0,t.to-t.from) && gap) *gap=true;
            if(wall<t.wall+std::max(0,t.to-t.from)) break;
            offset=t.to;
        }
        stamp=wall-offset;return true;
    }
private:
    struct Transition { int64_t wall; int from,to; };
    bool Build(const Zone& zone,int year,std::vector<Transition>& out) {
        for(const auto& shift:zone.shifts) {
            Date start; int from,to;
            if(!ParseDate(Get(shift,"DTSTART").value,start)||start.day||start.utc||
               !Offset(Get(shift,"TZOFFSETFROM").value,from)||!Offset(Get(shift,"TZOFFSETTO").value,to)) return false;
            out.push_back({Wall(start),from,to});
            const auto& spec=Get(shift,"RRULE").value;
            if(!spec.empty()) {
                Rule rule;if(!ParseRule(spec,rule)||rule.frequency!="YEARLY") return false;
                bool ok=Expand(start,rule,Days(year-1,1,1),Days(year+1,12,31),budget_,[&](const Date& d) {
                    if(!rule.until.y||!rule.until.utc||Wall(d)-from<=Wall(rule.until)) out.push_back({Wall(d),from,to});
                    return true;
                });
                if(!ok) return false;
            }
            if(!Dates(shift,"RDATE",[&](const Property& p) { Date d;if(!ParseDate(p.value,d)||d.day||d.utc) return false;out.push_back({Wall(d),from,to});return true; })) return false;
        }
        std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.wall<b.wall;}); return !out.empty();
    }
    const Document& doc_;unsigned& budget_;
    std::map<std::pair<std::string,int>,std::vector<Transition>> transitions_;
};
struct Event {
    std::string uid,title,zone;Date date;
    time_t start=0, end=0, recurrence=0;
    bool override=false,cancelled=false,nominal=false;
    int nominal_days=0; int64_t seconds=0;
    Rule rule; bool repeats=false;
    std::set<time_t> excludes, extras;
};
bool Duration(const std::string& s,int& days,int64_t& seconds) {
    if(s.size()<2||s.size()>32||s[0]!='P') return false;
    size_t at=1;bool clock=false,any=false;int rank=0;
    while(at<s.size()) {
        if(s[at]=='T') { if(clock||at+1==s.size()) return false;clock=true;++at;continue; }
        int n=0;size_t first=at;
        while(at<s.size()&&s[at]>='0'&&s[at]<='9') { n=n*10+s[at++]-'0';if(n>31536000) return false; }
        if(first==at||at==s.size()) return false;
        const char unit=s[at++];int next=0;
        if(unit=='W'&&!clock&&first==1&&at==s.size()) { days=n*7;return days<=366; }
        if(unit=='D'&&!clock) { days=n;next=1; }
        else if(unit=='H'&&clock) { seconds+=int64_t(n)*3600;next=2; }
        else if(unit=='M'&&clock) { seconds+=int64_t(n)*60;next=3; }
        else if(unit=='S'&&clock) { seconds+=n;next=4; }
        else return false;
        if(next<=rank) return false;
        rank=next;any=true;
    }
    return any&&days<=366&&seconds<=366LL*86400;
}
bool ReadEvent(const Fields& f,Times& times,Event& e) {
    e.uid=Get(f,"UID").value; e.title=Unescape(Get(f,"SUMMARY").value);
    e.cancelled=Upper(Get(f,"STATUS").value)=="CANCELLED";
    const auto& rid=Get(f,"RECURRENCE-ID");
    if(!rid.value.empty()) { Date d;e.override=true;if(!rid.range.empty()||!times.Convert(rid,d,e.recurrence)) return false; }
    if(e.uid.empty()||e.uid.size()>256||f.count("EXRULE")) return false;
    if(e.cancelled) return true;
    const auto& start=Get(f,"DTSTART"); e.zone=start.zone;
    if(!times.Convert(start,e.date,e.start)) return false;
    e.nominal=e.date.day; e.nominal_days=e.date.day?1:0;
    const auto& end=Get(f,"DTEND"); const auto& duration=Get(f,"DURATION").value;
    if(!end.value.empty()) {
        Date d;if(!duration.empty()||!times.Convert(end,d,e.end)||d.day!=e.date.day||e.end<=e.start) return false;
        if(e.date.day) e.nominal_days=Days(d.y,d.m,d.d)-Days(e.date.y,e.date.m,e.date.d);
        else e.seconds=e.end-e.start;
    } else if(!duration.empty()) {
        e.nominal=true;e.nominal_days=0;
        if(!Duration(duration,e.nominal_days,e.seconds)||(e.date.day&&e.seconds)) return false;
    }
    if(e.nominal_days>366||e.seconds>366LL*86400) return false;
    const auto& spec=Get(f,"RRULE").value;
    if(!spec.empty()) {
        if(e.override||!ParseRule(spec,e.rule)) return false;
        if(e.rule.until.y&&(e.rule.until.day!=e.date.day||(!e.date.day&&(e.date.utc||!e.zone.empty())&&!e.rule.until.utc))) return false;
        e.repeats=true;
    }
    auto dates=[&](const char* key,std::set<time_t>& values) {
        return Dates(f,key,[&](const Property& p) { Date d;time_t stamp;if(!times.Convert(p,d,stamp)||d.day!=e.date.day) return false;values.insert(stamp);return values.size()<=512; });
    };
    return dates("EXDATE",e.excludes)&&dates("RDATE",e.extras);
}
time_t LocalMidnight(time_t t) { tm d{};localtime_r(&t,&d);d.tm_hour=d.tm_min=d.tm_sec=0;d.tm_isdst=-1;return mktime(&d); }
}
bool ValidCalendarUrl(const std::string& url) {
    const std::string prefix="https://calendar.google.com/calendar/ical/",suffix="/basic.ics";
    if(url.size()>kCalendarUrlLimit||url.compare(0,prefix.size(),prefix)!=0||url.size()<prefix.size()+suffix.size()) return false;
    if(url.compare(url.size()-suffix.size(),suffix.size(),suffix)!=0) return false;
    const auto rest=url.substr(prefix.size(),url.size()-prefix.size()-suffix.size());
    const auto slash=rest.find('/');
    if(slash==rest.npos||slash==0||rest.find('/',slash+1)!=rest.npos) return false;
    const auto secret=rest.substr(slash+1);
    if(secret.compare(0,8,"private-")!=0||secret.size()<24||secret.size()>136) return false;
    for(size_t i=8;i<secret.size();++i) if(!std::isxdigit(static_cast<unsigned char>(secret[i]))) return false;
    const auto id=rest.substr(0,slash);
    for(size_t i=0;i<id.size();++i) {
        const unsigned char c=id[i];
        if(std::isalnum(c)||c=='@'||c=='.'||c=='_'||c=='-'||c=='+') continue;
        if(c=='%'&&i+2<id.size()&&std::isxdigit(static_cast<unsigned char>(id[i+1]))&&std::isxdigit(static_cast<unsigned char>(id[i+2]))) { i+=2;continue; }
        return false;
    }
    return true;
}
bool ParseCalendar(const std::string& body,time_t now,Calendar& out) {
    if(now<1767225600) return false;
    Document doc;if(!ReadDocument(body,doc)) return false;
    unsigned budget=1000000; Times times(doc,budget);
    Calendar next;next.configured=true;next.valid=true;next.updated=now;
    const time_t today=LocalMidnight(now);tm end{};localtime_r(&today,&end);end.tm_mday+=kHorizon;end.tm_isdst=-1;
    next.through=mktime(&end);
    const int first_day=int(today/86400)-367,last_day=int(next.through/86400)+2;
    std::vector<Event> events;std::set<std::string> blocked;
    std::set<std::pair<std::string,time_t>> identities,overrides;
    for(const auto& fields:doc.events) {
        Event e;
        if(!ReadEvent(fields,times,e)) { ++next.skipped;blocked.insert(Get(fields,"UID").value);continue; }
        if(!identities.insert({e.uid,e.override?e.recurrence:0}).second) return false;
        if(e.override) overrides.insert({e.uid,e.recurrence});
        events.push_back(std::move(e));
    }
    for(const auto& e:events) {
        if(e.cancelled) continue;
        if(blocked.count(e.uid)) { ++next.skipped;continue; }
        std::vector<CalendarEvent> occurrences; std::set<time_t> emitted;
        bool valid=true;
        auto add=[&](const Date& d,time_t stamp) {
            if(e.excludes.count(stamp)||(!e.override&&overrides.count({e.uid,stamp}))||!emitted.insert(stamp).second) return true;
            time_t finish=stamp+e.seconds;
            if(e.nominal) {
                Date finish_date=Civil(Days(d.y,d.m,d.d)+e.nominal_days);
                finish_date.h=d.h;finish_date.min=d.min;finish_date.sec=d.sec;finish_date.day=d.day;finish_date.utc=d.utc;
                if(!times.Convert(finish_date,e.zone,finish)) { valid=false;return false; }
                finish+=e.seconds;
            }
            if(stamp<next.through&&(finish>today||(finish==stamp&&stamp>=today))) {
                if(occurrences.size()>=kMaxOccurrences) { valid=false;return false; }
                occurrences.push_back({e.title,stamp,finish,e.date.day});
            }
            return true;
        };
        add(e.date,e.start);
        if(e.repeats) {
            const bool expanded=Expand(e.date,e.rule,first_day,last_day,budget,[&](const Date& d) {
                time_t stamp;bool gap=false;
                if(!times.Convert(d,e.zone,stamp,&gap)) { valid=false;return false; }
                if(e.rule.until.y&&e.rule.until.utc&&stamp>Wall(e.rule.until)) return false;
                if(gap) return true;
                return add(d,stamp);
            }, [&](const Date& d) {
                time_t stamp; bool gap=false;
                if(!times.Convert(d,e.zone,stamp,&gap)) { valid=false; return false; }
                return !gap;
            });
            valid &= expanded;
        }
        // RDATE retains its own local value, needed for all-day/nominal duration.
        for(time_t stamp:e.extras) {
            if(e.nominal&&!e.date.day&& !e.zone.empty() && e.zone!="America/New_York" && e.zone!="US/Eastern") { valid=false;break; }
            tm local{};
            if(e.date.utc) gmtime_r(&stamp,&local);else localtime_r(&stamp,&local);
            Date d{local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,local.tm_sec,e.date.day,e.date.utc};
            add(d,stamp);
        }
        if(!valid) { ++next.skipped;continue; }
        next.events.insert(next.events.end(),occurrences.begin(),occurrences.end());
        // Keep memory bounded even with thousands of series; retain nearest dates.
        if(next.events.size()>kMaxOccurrences) {
            std::sort(next.events.begin(),next.events.end(),[](const auto& a,const auto& b){return a.start<b.start;});
            next.events.resize(kMaxOccurrences);++next.skipped;
        }
    }
    std::stable_sort(next.events.begin(),next.events.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    out=std::move(next);return true;
}
Agenda SelectAgenda(const Calendar& calendar,time_t now) {
    Agenda result;if(!calendar.valid||now<1767225600||now>=calendar.through) return result;
    const time_t today=LocalMidnight(now);
    tm tomorrow{};localtime_r(&today,&tomorrow);++tomorrow.tm_mday;tomorrow.tm_isdst=-1;
    const time_t end=mktime(&tomorrow);
    // The parser keeps occurrences sorted by start time. Fill spare rows with
    // later days even while today's last appointment is still in progress.
    for(const auto& e:calendar.events) {
        if (e.end <= now && !(e.start == e.end && e.start >= now)) continue;
        if (result.events.size() == 3) { ++result.additional; continue; }
        result.events.push_back(&e);
        if (e.start < end) result.today = true;
    }
    return result;
}
std::string CalendarEventTime(const CalendarEvent& e,bool today,time_t now) {
    if (today && !e.all_day && e.start < LocalMidnight(now)) today = false;
    tm local{};localtime_r(&e.start,&local);char b[64];
    if(e.all_day) { if(today) return "All day";strftime(b,sizeof(b),"%a %m/%d | All day",&local); }
    else strftime(b,sizeof(b),today?"%I:%M%p":"%a %m/%d | %I:%M%p",&local);
    return b;
}
}
