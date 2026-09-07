#include "network.h"
#include "dashboard.h"
#include "settings.h"
#include "app_events.h"
#include "power_manager.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace home {
namespace {
std::atomic<bool> connected{false}, reconnect{false}, connecting{false};
std::string form_token;
TaskHandle_t reconnect_task = nullptr;
void Reconnect(void*) {
    unsigned delay = 1;
    int64_t next_attempt = 0, connection_deadline = 0;
    while (true) {
        if (!reconnect || connected) {
            delay = 1; next_attempt = 0;
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        const auto now = esp_timer_get_time();
        if (connecting) {
            if (now >= connection_deadline) {
                connecting = false;
                esp_wifi_disconnect(); // Bound association/DHCP stalls.
            } else {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(std::max<int64_t>(1, (connection_deadline - now) / 1000)));
                continue;
            }
        }
        if (now < next_attempt) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(std::max<int64_t>(1, (next_attempt - now) / 1000)));
            continue;
        }
        connecting = true;
        connection_deadline = now + 30000000;
        if (esp_wifi_connect() != ESP_OK) connecting = false;
        // Disconnect notifications cannot bypass this monotonic deadline.
        next_attempt = esp_timer_get_time() + int64_t(delay) * 1000000;
        delay = std::min(delay * 2, 300u);
    }
}
httpd_handle_t server = nullptr;
std::string Read(nvs_handle_t h, const char* key) {
    size_t size = 0;
    if (nvs_get_str(h, key, nullptr, &size) != ESP_OK || size == 0 || size > 4096) return {};
    std::string s(size, '\0');
    if (nvs_get_str(h, key, s.data(), &size) != ESP_OK) return {};
    s.resize(size - 1); return s;
}
Settings ReadSettings(nvs_handle_t h) {
    Settings s;
    const auto saved = Read(h, "settings_v1");
    if (!saved.empty()) { DecodeSettings(saved, s); return s; }
    s.ssid = Read(h, "ssid"); s.password = Read(h, "password");
    return s;
}
void Event(void*, esp_event_base_t base, int32_t id, void*) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        connected = true; connecting = false; WakeApp();
        if (reconnect_task) xTaskNotifyGive(reconnect_task);
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false; connecting = false;
        WakeApp();
        if (reconnect && reconnect_task) xTaskNotifyGive(reconnect_task);
    }
}
std::string RandomHex() {
    uint32_t r[2]; esp_fill_random(r, sizeof(r));
    char b[17]; snprintf(b, sizeof(b), "%08lx%08lx", static_cast<unsigned long>(r[0]), static_cast<unsigned long>(r[1]));
    return b;
}
extern const char setup_start[] asm("_binary_setup_html_start");
extern const char setup_end[] asm("_binary_setup_html_end");
esp_err_t GetSetup(httpd_req_t* req) {
    std::string html(setup_start, setup_end - setup_start);
    auto pos = html.find("__TOKEN__");
    if (pos != std::string::npos) html.replace(pos, 9, form_token);
    nvs_handle_t h;
    Settings saved;
    if (nvs_open("home_note", NVS_READONLY, &h) == ESP_OK) { saved = ReadSettings(h); nvs_close(h); }
    auto replace = [&](const char* key, const char* value) {
        auto at = html.find(key); if (at != html.npos) html.replace(at, strlen(key), value);
    };
    replace("__HAS_WIFI__", ValidCredentials(saved.ssid, saved.password) ? "true" : "false");
    replace("__HAS_CALENDAR__", saved.calendar_url.empty() ? "false" : "true");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    return httpd_resp_send(req, html.c_str(), html.size());
}
void Restart(void*) { vTaskDelay(pdMS_TO_TICKS(1500)); esp_restart(); }
esp_err_t SaveSetup(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (req->content_len <= 0 || req->content_len > 4096) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size");
    std::string body(req->content_len, '\0'); size_t offset = 0;
    while (offset < body.size()) {
        int n = httpd_req_recv(req, body.data() + offset, body.size() - offset);
        if (n <= 0) return ESP_FAIL;
        offset += n;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("home_note", NVS_READWRITE, &h);
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not open settings");
    Settings next; std::string error;
    if (!ApplySetup(body, form_token, ReadSettings(h), next, error)) {
        nvs_close(h);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error.c_str());
    }
    // One versioned value avoids partially replacing Wi-Fi/calendar settings.
    const auto encoded = EncodeSettings(next);
    err = encoded.empty() ? ESP_ERR_NO_MEM : nvs_set_str(h, "settings_v1", encoded.c_str());
    if (err == ESP_OK) err = nvs_set_u8(h, "setup", 0);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    // Never log request bodies, Wi-Fi credentials, or calendar links.
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save settings");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Saved. Your NOTE4 is restarting and will try your Wi-Fi. Reconnect your phone to your home network. If Wi-Fi is unavailable, the device will retry. Hold OK to reopen setup with the same setup password.");
    xTaskCreate(Restart, "restart", 2048, nullptr, 3, nullptr);
    return ESP_OK;
}
}
esp_err_t InitNetwork() {
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE); // This component can log full URLs, which contain the calendar secret.
    esp_err_t err = nvs_flash_init(); // Do not erase factory NVS on an error.
    if (err != ESP_OK) return err;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, Event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, Event, nullptr));
    if (xTaskCreate(Reconnect, "wifi_reconnect", 3072, nullptr, 2, &reconnect_task) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
bool HasWifiCredentials() {
    nvs_handle_t h; if (nvs_open("home_note", NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t setup = 0; nvs_get_u8(h, "setup", &setup);
    const auto saved = ReadSettings(h);
    bool valid = ValidCredentials(saved.ssid, saved.password);
    nvs_close(h); return valid && !setup;
}
bool ConnectWifi(unsigned timeout_ms) {
    nvs_handle_t h; if (nvs_open("home_note", NVS_READONLY, &h) != ESP_OK) return false;
    const auto saved = ReadSettings(h); nvs_close(h);
    const auto& ssid = saved.ssid; const auto& pass = saved.password;
    if (!ValidCredentials(ssid, pass)) return false;
    wifi_config_t cfg = {};
    memcpy(cfg.sta.ssid, ssid.data(), ssid.size());
    memcpy(cfg.sta.password, pass.data(), pass.size());
    cfg.sta.threshold.authmode = pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    reconnect = true;
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    xTaskNotifyGive(reconnect_task);
    unsigned waited = 0;
    while (!connected && waited < timeout_ms) { vTaskDelay(pdMS_TO_TICKS(100)); waited += 100; }
    return connected;
}
bool WifiConnected() { return connected; }
esp_err_t StartSetup(SetupInfo& info) {
    reconnect = false; connected = false; xTaskNotifyGive(reconnect_task); esp_wifi_stop();
    esp_netif_create_default_wifi_ap();
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char name[32]; snprintf(name, sizeof(name), "HomeNote4-%02X%02X", mac[4], mac[5]);
    info.ssid = name; form_token = RandomHex();
    nvs_handle_t h;
    auto err = nvs_open("home_note", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    size_t password_size = 0;
    err = nvs_get_str(h, "setup_psk", nullptr, &password_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        info.password = RandomHex().substr(0, 12);
        err = nvs_set_str(h, "setup_psk", info.password.c_str());
        if (err == ESP_OK) err = nvs_commit(h);
    } else if (err == ESP_OK) {
        if (password_size != 13) err = ESP_ERR_INVALID_STATE;
        else {
            char password[13] = {};
            err = nvs_get_str(h, "setup_psk", password, &password_size);
            if (err == ESP_OK) {
                info.password.assign(password, 12);
                if (info.password.find_first_not_of("0123456789abcdef") != std::string::npos) err = ESP_ERR_INVALID_STATE;
            }
        }
    }
    nvs_close(h);
    if (err != ESP_OK) return err;
    wifi_config_t cfg = {};
    memcpy(cfg.ap.ssid, info.ssid.data(), info.ssid.size()); cfg.ap.ssid_len = info.ssid.size();
    memcpy(cfg.ap.password, info.password.data(), info.password.size());
    cfg.ap.channel = 1; cfg.ap.authmode = WIFI_AUTH_WPA2_PSK; cfg.ap.max_connection = 2;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG(); hc.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&server, &hc));
    httpd_uri_t get = {}; get.uri = "/"; get.method = HTTP_GET; get.handler = GetSetup;
    httpd_uri_t save = {}; save.uri = "/save"; save.method = HTTP_POST; save.handler = SaveSetup;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get));
    return httpd_register_uri_handler(server, &save);
}
void SetupOnNextBoot() {
    nvs_handle_t h;
    if (nvs_open("home_note", NVS_READWRITE, &h) != ESP_OK) return;
    esp_err_t err = nvs_set_u8(h, "setup", 1);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) esp_restart();
}
void StartClock() {
    setenv("TZ", kTimezone, 1); tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org"); esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();
}
static bool FetchBody(const char* url, std::string& body, size_t max_bytes, const char* accept) {
    // Refuse TLS until a credible clock is available; certificate checking stays enabled.
    if (!connected || time(nullptr) < 1767225600) return false;
    const auto started = esp_timer_get_time();
    esp_http_client_config_t cfg = {}; cfg.url = url; cfg.timeout_ms = 12000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    // ESPN accepts this standard library compatibility header, but rejects
    // the prior custom app User-Agent with HTTP 403. No authentication is used.
    cfg.user_agent = "Python-urllib/3.10";
    cfg.buffer_size = 4096;
    cfg.disable_auto_redirect = true;
    auto h = esp_http_client_init(&cfg); if (!h) { CountNetwork(0, 0, false); return false; }
    esp_http_client_set_header(h, "Accept", accept);
    esp_http_client_set_header(h, "Accept-Encoding", "identity");
    bool ok = esp_http_client_open(h, 0) == ESP_OK;
    if (ok) { esp_http_client_fetch_headers(h); ok = esp_http_client_get_status_code(h) == 200;
        if (!ok) ESP_LOGW("home_http", "HTTP status %d", esp_http_client_get_status_code(h)); }
    body.clear(); char buf[4096];
    const int64_t deadline = esp_timer_get_time() + 45000000;
    while (ok) {
        int n = esp_http_client_read(h, buf, sizeof(buf));
        if (n < 0) { ok = false; break; }
        if (n == 0) { ok = esp_http_client_is_complete_data_received(h); break; }
        if (body.size() + n > max_bytes || esp_timer_get_time() > deadline) { ok = false; break; }
        body.append(buf, n);
    }
    esp_http_client_close(h); esp_http_client_cleanup(h);
    CountNetwork(body.size(), (esp_timer_get_time() - started) / 1000, ok && !body.empty());
    return ok && !body.empty();
}
bool FetchJson(const char* url, std::string& body, size_t max_bytes) {
    return FetchBody(url, body, max_bytes, "application/json");
}
std::string CalendarUrl() {
    nvs_handle_t h; if (nvs_open("home_note", NVS_READONLY, &h) != ESP_OK) return {};
    const auto s = ReadSettings(h); nvs_close(h); return s.calendar_url;
}
bool FetchCalendar(const std::string& url, std::string& body) {
    return ValidCalendarUrl(url) && FetchBody(url.c_str(), body, 3 * 1024 * 1024, "text/calendar");
}
}
