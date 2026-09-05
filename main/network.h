#pragma once
#include "esp_err.h"
#include <string>
namespace home {
struct SetupInfo { std::string ssid, password; };
esp_err_t InitNetwork();
bool HasWifiCredentials();
bool ConnectWifi(unsigned timeout_ms);
bool WifiConnected();
esp_err_t StartSetup(SetupInfo& info);
void SetupOnNextBoot();
bool FetchJson(const char* url, std::string& body);
void StartClock();
}
