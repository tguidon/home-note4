#include "network.h"
#include "dashboard.h"
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
std::atomic<bool> connected{false}, reconnect{false};
std::string form_token;
httpd_handle_t server = nullptr;
std::string Read(nvs_handle_t h, const char* key) {
    size_t size = 0;
    if (nvs_get_str(h, key, nullptr, &size) != ESP_OK || size > 128) return {};
    std::string s(size, '\0');
    if (nvs_get_str(h, key, s.data(), &size) != ESP_OK) return {};
    s.resize(size - 1); return s;
}
void Event(void*, esp_event_base_t base, int32_t id, void*) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) connected = true;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        if (reconnect) esp_wifi_connect();
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
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    return httpd_resp_send(req, html.c_str(), html.size());
}
void Restart(void*) { vTaskDelay(pdMS_TO_TICKS(1500)); esp_restart(); }
esp_err_t SaveSetup(httpd_req_t* req) {
    if (req->content_len <= 0 || req->content_len > 768) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size");
    std::string body(req->content_len, '\0'); size_t offset = 0;
    while (offset < body.size()) {
        int n = httpd_req_recv(req, body.data() + offset, body.size() - offset);
        if (n <= 0) return ESP_FAIL;
        offset += n;
    }
    cJSON* j = cJSON_Parse(body.c_str());
    auto text = [&](const char* key) -> std::string {
        auto* v = cJSON_GetObjectItemCaseSensitive(j, key);
        return cJSON_IsString(v) ? v->valuestring : "";
    };
    std::string ssid = text("ssid"), password = text("password"), token = text("token");
    cJSON_Delete(j);
    if (token != form_token) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Reload the setup page");
    if (!ValidCredentials(ssid, password)) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Use a 1-32 byte network name and an 8-63 byte password, or leave password empty for an open network.");
    nvs_handle_t h;
    esp_err_t err = nvs_open("home_note", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "ssid", ssid.c_str());
        if (err == ESP_OK) err = nvs_set_str(h, "password", password.c_str());
        if (err == ESP_OK) err = nvs_set_u8(h, "setup", 0);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    // Never log request bodies or Wi-Fi credentials.
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save settings");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Saved. Your NOTE4 is restarting and will try your Wi-Fi. Reconnect your phone to your home network. If connection fails, the screen will show a new setup password.");
    xTaskCreate(Restart, "restart", 2048, nullptr, 3, nullptr);
    return ESP_OK;
}
}
esp_err_t InitNetwork() {
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
    return ESP_OK;
}
bool HasWifiCredentials() {
    nvs_handle_t h; if (nvs_open("home_note", NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t setup = 0; nvs_get_u8(h, "setup", &setup);
    bool valid = ValidCredentials(Read(h, "ssid"), Read(h, "password"));
    nvs_close(h); return valid && !setup;
}
bool ConnectWifi(unsigned timeout_ms) {
    nvs_handle_t h; if (nvs_open("home_note", NVS_READONLY, &h) != ESP_OK) return false;
    std::string ssid = Read(h, "ssid"), pass = Read(h, "password"); nvs_close(h);
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
    ESP_ERROR_CHECK(esp_wifi_connect());
    unsigned waited = 0;
    while (!connected && waited < timeout_ms) { vTaskDelay(pdMS_TO_TICKS(100)); waited += 100; }
    return connected;
}
bool WifiConnected() { return connected; }
esp_err_t StartSetup(SetupInfo& info) {
    reconnect = false; connected = false; esp_wifi_stop();
    esp_netif_create_default_wifi_ap();
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char name[32]; snprintf(name, sizeof(name), "HomeNote4-%02X%02X", mac[4], mac[5]);
    info.ssid = name; info.password = RandomHex().substr(0, 12); form_token = RandomHex();
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
bool FetchJson(const char* url, std::string& body, size_t max_bytes) {
    // Refuse TLS until a credible clock is available; certificate checking stays enabled.
    if (!connected || time(nullptr) < 1767225600) return false;
    esp_http_client_config_t cfg = {}; cfg.url = url; cfg.timeout_ms = 12000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    // ESPN accepts this standard library compatibility header, but rejects
    // the prior custom app User-Agent with HTTP 403. No authentication is used.
    cfg.user_agent = "Python-urllib/3.10";
    cfg.buffer_size = 4096;
    auto h = esp_http_client_init(&cfg); if (!h) return false;
    esp_http_client_set_header(h, "Accept", "application/json");
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
    return ok && !body.empty();
}
}
