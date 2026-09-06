#include "calendar.h"
#include "calendar_rules.h"
#include "settings.h"
#include "dashboard.h"
#include "render.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
using namespace home;
std::string Feed(const std::string& s) { return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"+s+"END:VCALENDAR\r\n"; }
std::string Event(const std::string& s) { return "BEGIN:VEVENT\r\n"+s+"END:VEVENT\r\n"; }
std::string Load(const char* path) { std::ifstream f(path);assert(f.good());return {std::istreambuf_iterator<char>(f),{}}; }
void Pbm(ZectrixCanvas& c,const std::string& path) { std::ofstream f(path,std::ios::binary);f<<"P4\n400 300\n";for(size_t i=0;i<c.size();++i)f.put(char(~c.data()[i])); }
int main(int argc,char** argv) {
    setenv("TZ",kTimezone,1);tzset();
    const auto now=ParseUtcTime("2026-09-05T16:00:00Z");
    const std::string url="https://calendar.google.com/calendar/ical/example%40gmail.com/private-0123456789abcdef0123456789abcdef/basic.ics";
    assert(ValidCalendarUrl(url));
    for(const auto& bad:{"http://calendar.google.com/calendar/ical/a/private-0123456789abcdef/basic.ics", "https://evil.example/calendar/ical/a/private-0123456789abcdef/basic.ics", "https://calendar.google.com.evil.example/calendar/ical/a/private-0123456789abcdef/basic.ics", "https://calendar.google.com/calendar/ical/a/public/basic.ics", "https://calendar.google.com/calendar/ical/a/private-0123456789abcdef/basic.ics?secret=1", "https://calendar.google.com/calendar/ical/a/../private-0123456789abcdef/basic.ics"}) assert(!ValidCalendarUrl(bad));
    assert(!ValidCalendarUrl(url+"\n"));assert(!ValidCalendarUrl(url+std::string(2000,'a')));
    Settings current{"Home Wi-Fi","password123",url},next;
    assert(DecodeSettings(EncodeSettings(current),next)&&next.calendar_url==url&&next.password==current.password);
    std::string error;
    assert(ApplySetup(R"({"token":"abc","wifi_action":"keep","calendar_action":"keep"})","abc",current,next,error));
    assert(next.ssid==current.ssid&&next.calendar_url==url);
    assert(!ApplySetup(R"({"token":"wrong","wifi_action":"keep","calendar_action":"remove"})","abc",current,next,error));
    assert(next.calendar_url==url);
    assert(!ApplySetup(R"({"token":"abc","token":"abc","wifi_action":"keep","calendar_action":"remove"})","abc",current,next,error));
    assert(!ApplySetup(R"({"token":"abc","wifi_action":"keep","calendar_action":"replace","calendar_url":"https://evil.example/"})","abc",current,next,error));
    assert(error.find("evil")==std::string::npos&&next.calendar_url==url);
    assert(ApplySetup(R"({"token":"abc","wifi_action":"keep","calendar_action":"remove"})","abc",current,next,error)&&next.calendar_url.empty()&&next.password==current.password);
    assert(!ApplySetup(R"({"token":"abc","wifi_action":"keep","calendar_action":"keep"})","abc",{},next,error));
    Calendar calendar;
    const auto simple=Feed(Event("UID:one\r\nDTSTART:20260905T140000Z\r\nDTEND:20260905T150000Z\r\nSUMMARY:Coffee\\, catch-up\r\n")+
        Event("UID:two\r\nDTSTART;VALUE=DATE:20260905\r\nDTEND;VALUE=DATE:20260906\r\nSUMMARY:Family day\r\n")+
        Event("UID:three\r\nDTSTART;TZID=America/New_York:20260907T090000\r\nDURATION:PT1H\r\nSUMMARY:Weekly planning with a long\r\n  title\r\n")+
        Event("UID:overnight\r\nDTSTART:20260905T020000Z\r\nDTEND:20260905T060000Z\r\nSUMMARY:Overnight\r\n"));
    assert(ParseCalendar(simple,now,calendar)&&calendar.valid&&calendar.skipped==0);
    auto agenda=SelectAgenda(calendar,now);assert(agenda.today&&agenda.events.size()==3);
    assert(agenda.events.back()->title=="Coffee, catch-up");
    agenda=SelectAgenda(calendar,ParseUtcTime("2026-09-06T16:00:00Z"));
    assert(!agenda.today&&agenda.events.size()==1&&agenda.events[0]->title=="Weekly planning with a long title");
    assert(CalendarEventTime(*agenda.events[0],false)=="Mon 09/07 | 09:00AM");
    auto saved=calendar;
    assert(!ParseCalendar(simple.substr(0,simple.size()-16),now,calendar)&&calendar.events.size()==saved.events.size());
    assert(!ParseCalendar("<html>sign in</html>",now,calendar));
    assert(!ParseCalendar(Feed("")+"junk",now,calendar));
    assert(ParseCalendar(Feed(""),now,calendar)&&calendar.events.empty()&&calendar.skipped==0);
    const std::string weekly=Feed(Event("UID:weekly\r\nDTSTART;TZID=America/New_York:20260901T090000\r\nDTEND;TZID=America/New_York:20260901T100000\r\nRRULE:FREQ=WEEKLY;BYDAY=TU,SA;COUNT=6\r\nEXDATE;TZID=America/New_York:20260905T090000\r\nSUMMARY:Workout\r\n")+
       Event("UID:weekly\r\nRECURRENCE-ID;TZID=America/New_York:20260908T090000\r\nDTSTART;TZID=America/New_York:20260908T110000\r\nDTEND;TZID=America/New_York:20260908T120000\r\nSUMMARY:Moved workout\r\n")+
       Event("UID:weekly\r\nRECURRENCE-ID;TZID=America/New_York:20260912T090000\r\nSTATUS:CANCELLED\r\n"));
    assert(ParseCalendar(weekly,now,calendar)&&!calendar.skipped);
    assert(calendar.events.size()==3&&calendar.events[0].title=="Moved workout");
    assert(calendar.events[0].start==ParseUtcTime("2026-09-08T15:00:00Z"));
    // Monthly last weekday and annual leap day skip invalid calendar dates.
    assert(ParseCalendar(Feed(Event("UID:month\r\nDTSTART:20260130T140000Z\r\nRRULE:FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1\r\nSUMMARY:Month end\r\n")),now,calendar));
    assert(!calendar.skipped&&calendar.events.size()==3);
    assert(calendar.events[0].start==ParseUtcTime("2026-09-30T14:00:00Z"));
    assert(calendar.events[1].start==ParseUtcTime("2026-10-30T14:00:00Z"));
    assert(ParseCalendar(Feed(Event("UID:leap\r\nDTSTART;VALUE=DATE:20240229\r\nRRULE:FREQ=YEARLY\r\nSUMMARY:Leap birthday\r\n")),ParseUtcTime("2028-02-01T12:00:00Z"),calendar));
    assert(!calendar.skipped&&calendar.events.size()==1);
    assert(ParseCalendar(Feed(Event("UID:daily\r\nDTSTART;TZID=America/New_York:20261031T090000\r\nRRULE:FREQ=DAILY;COUNT=3\r\nSUMMARY:DST meeting\r\n")),ParseUtcTime("2026-10-31T12:00:00Z"),calendar));
    assert(calendar.events.size()==3);
    assert(calendar.events[0].start==ParseUtcTime("2026-10-31T13:00:00Z"));
    assert(calendar.events[1].start==ParseUtcTime("2026-11-01T14:00:00Z"));
    assert(ParseCalendar(Feed(Event("UID:until\r\nDTSTART;TZID=America/New_York:20260901T090000\r\nRRULE:FREQ=DAILY;UNTIL=20260906T130000Z\r\nSUMMARY:Until inclusive\r\n")),now,calendar)&&calendar.events.size()==2);
    assert(ParseCalendar(Feed(Event("UID:extra\r\nDTSTART;VALUE=DATE:20260905\r\nRDATE;VALUE=DATE:20260908,20260910\r\nEXDATE;VALUE=DATE:20260908\r\nSUMMARY:Extra dates\r\n")),now,calendar)&&calendar.events.size()==2);
    assert(ParseCalendar(Feed(Event("UID:spring\r\nDTSTART;TZID=America/New_York:20260307T023000\r\nRRULE:FREQ=DAILY;COUNT=3\r\nSUMMARY:Skip spring gap\r\n")),ParseUtcTime("2026-03-07T05:00:00Z"),calendar));
    assert(!calendar.skipped&&calendar.events.size()==3);
    assert(calendar.events[1].start==ParseUtcTime("2026-03-09T06:30:00Z"));
    assert(calendar.events[2].start==ParseUtcTime("2026-03-10T06:30:00Z"));
    assert(ParseCalendar(Feed(Event("UID:fold\r\nDTSTART;TZID=America/New_York:20261101T013000\r\nSUMMARY:First fold occurrence\r\n")),now,calendar));
    assert(calendar.events.size()==1&&calendar.events[0].start==ParseUtcTime("2026-11-01T05:30:00Z"));
    assert(ParseCalendar(Feed(Event("UID:all-day-dst\r\nDTSTART;VALUE=DATE:20261101\r\nSUMMARY:25 hour day\r\n")),now,calendar));
    assert(calendar.events[0].end-calendar.events[0].start==25*3600);
    const std::string zone="BEGIN:VTIMEZONE\r\nTZID:Europe/London\r\nBEGIN:DAYLIGHT\r\nDTSTART:19700329T010000\r\nTZOFFSETFROM:+0000\r\nTZOFFSETTO:+0100\r\nRRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=-1SU\r\nEND:DAYLIGHT\r\nBEGIN:STANDARD\r\nDTSTART:19701025T020000\r\nTZOFFSETFROM:+0100\r\nTZOFFSETTO:+0000\r\nRRULE:FREQ=YEARLY;BYMONTH=10;BYDAY=-1SU\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n";
    assert(ParseCalendar(Feed(zone+Event("UID:london\r\nDTSTART;TZID=Europe/London:20261024T090000\r\nRRULE:FREQ=DAILY;COUNT=3\r\nSUMMARY:London meeting\r\n")),now,calendar));
    assert(!calendar.skipped&&calendar.events.size()==3);
    assert(calendar.events[0].start==ParseUtcTime("2026-10-24T08:00:00Z"));
    assert(calendar.events[1].start==ParseUtcTime("2026-10-25T09:00:00Z"));
    assert(ParseCalendar(Feed(Event("UID:bad\r\nDTSTART:20260905T140000Z\r\nRRULE:FREQ=HOURLY\r\nSUMMARY:Unsupported\r\n")),now,calendar));
    assert(calendar.skipped==1&&calendar.events.empty());
    assert(ParseCalendar(Feed(Event("UID:bad-zone\r\nDTSTART;TZID=Unknown/Zone:20260905T140000\r\nSUMMARY:Unknown time zone\r\n")),now,calendar)&&calendar.skipped==1);
    // An unsupported moved instance suppresses its whole series instead of displaying false appointments.
    assert(ParseCalendar(Feed(Event("UID:ranged\r\nDTSTART:20260905T140000Z\r\nRRULE:FREQ=DAILY;COUNT=3\r\nSUMMARY:Series\r\n")+Event("UID:ranged\r\nRECURRENCE-ID;RANGE=THISANDFUTURE:20260906T140000Z\r\nDTSTART:20260906T160000Z\r\nSUMMARY:Shift series\r\n")),now,calendar)&&calendar.skipped&&calendar.events.empty());
    for(int i=0;i<150;++i) { auto damaged=simple;damaged.resize(i);assert(!ParseCalendar(damaged,now,calendar)); }
    if(argc>1) {
        Dashboard d;assert(ParseWeather(Load("tests/fixtures/weather.json"),d.weather));d.weather_updated=now;
        assert(ParseCalendar(simple,now,d.calendar));ZectrixCanvas canvas;
        RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-today.pbm");
        RenderDashboard(canvas,d,0,ParseUtcTime("2026-09-06T16:00:00Z"),false,65);Pbm(canvas,std::string(argv[1])+"/calendar-next.pbm");
        d.calendar.error=true;RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-stale.pbm");
        assert(ParseCalendar(Feed(""),now,d.calendar));RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-empty.pbm");
        d.calendar={};d.calendar.configured=true;RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-loading.pbm");
        d.calendar={};RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-setup.pbm");
        std::string many;
        for(int i=0;i<8;++i) many+=Event("UID:"+std::to_string(i)+"\r\nDTSTART:20260905T140000Z\r\nSUMMARY:Meeting "+std::to_string(i+1)+"\r\n");
        assert(ParseCalendar(Feed(many),now,d.calendar));
        assert(SectionPages(d,0,now)==3&&SectionStart(d,1,now)==3&&PageCount(d,now)==6);
        for(int page=0;page<3;++page) { RenderDashboard(canvas,d,page,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-page-"+std::to_string(page)+".pbm"); }
        assert(SectionPages(d,0,now+86400)==1);
        d.calendar.skipped=1;RenderDashboard(canvas,d,0,now,true,87);Pbm(canvas,std::string(argv[1])+"/calendar-warning.pbm");
    }
    std::cout<<"PASS: private URL/settings, recurrence/exceptions, all-day/overnight, DST/timezones, next events, failure preservation, and home pagination\n";
}
