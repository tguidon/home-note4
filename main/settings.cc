#include "settings.h"
#include "calendar.h"
#include "dashboard.h"
#include "cJSON.h"
#include <cstring>
#include <set>
namespace home {
namespace {
cJSON* Object(const std::string& s) {
    if(s.empty()||s.size()>4096||s.find('\0')!=s.npos||s.find("\\u0000")!=s.npos) return nullptr;
    const char* end=nullptr;
    auto* j=cJSON_ParseWithLengthOpts(s.c_str(),s.size()+1,&end,true);
    if(!cJSON_IsObject(j)) { cJSON_Delete(j);return nullptr; }
    std::set<std::string> keys;
    for(auto* p=j->child;p;p=p->next) if(!p->string||!keys.insert(p->string).second) { cJSON_Delete(j);return nullptr; }
    return j;
}
bool Text(cJSON* j,const char* key,std::string& out) {
    const auto* value=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsString(value)) return false;
    out=value->valuestring;return true;
}
}
bool DecodeSettings(const std::string& body,Settings& out) {
    auto* j=Object(body);Settings s;
    bool ok=Text(j,"ssid",s.ssid)&&Text(j,"password",s.password)&&Text(j,"calendar_url",s.calendar_url);
    cJSON_Delete(j);
    if(!ok||!ValidCredentials(s.ssid,s.password)||(!s.calendar_url.empty()&&!ValidCalendarUrl(s.calendar_url))) return false;
    out=std::move(s);return true;
}
std::string EncodeSettings(const Settings& s) {
    auto* j=cJSON_CreateObject();if(!j) return {};
    const bool ok=cJSON_AddStringToObject(j,"ssid",s.ssid.c_str())&&cJSON_AddStringToObject(j,"password",s.password.c_str())&&cJSON_AddStringToObject(j,"calendar_url",s.calendar_url.c_str());
    char* raw=ok?cJSON_PrintUnformatted(j):nullptr;std::string out=raw?raw:"";
    cJSON_free(raw);cJSON_Delete(j);return out;
}
bool ApplySetup(const std::string& body,const std::string& token,const Settings& current,Settings& next,std::string& error) {
    auto* j=Object(body);Settings candidate=current;
    std::string sent,wifi,calendar,url;
    error="Reload the setup page and try again.";
    bool ok=Text(j,"token",sent)&&!token.empty()&&sent==token&&Text(j,"wifi_action",wifi)&&Text(j,"calendar_action",calendar);
    if(ok) {
        if(wifi=="replace") ok=Text(j,"ssid",candidate.ssid)&&Text(j,"password",candidate.password);
        else ok=wifi=="keep";
        ok=ok&&ValidCredentials(candidate.ssid,candidate.password);
        if(!ok) error="Enter a Wi-Fi name (1-32 bytes) and password (8-63 bytes), or leave the password blank for an open network.";
    }
    if(ok) {
        if(calendar=="replace") {
            ok=Text(j,"calendar_url",url);
            const auto first=url.find_first_not_of(" \t\r\n"),last=url.find_last_not_of(" \t\r\n");
            url=first==url.npos?"":url.substr(first,last-first+1);
            ok=ok&&ValidCalendarUrl(url);
            if(ok) candidate.calendar_url=url;
        } else if(calendar=="remove") candidate.calendar_url.clear();
        else ok=calendar=="keep";
        if(!ok) error="Use Google's Secret address in iCal format, starting with https://calendar.google.com/calendar/ical/ and ending in /basic.ics. Public links are not accepted.";
    }
    cJSON_Delete(j);
    if(ok) { next=std::move(candidate);error.clear(); }
    return ok;
}
}
