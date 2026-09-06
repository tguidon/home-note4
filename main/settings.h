#pragma once
#include <string>
namespace home {
struct Settings { std::string ssid,password,calendar_url; };
bool DecodeSettings(const std::string&,Settings&);
std::string EncodeSettings(const Settings&);
// The caller writes only on success. Errors never contain submitted values.
bool ApplySetup(const std::string& json,const std::string& token,const Settings& current,
                Settings& next,std::string& error);
}
