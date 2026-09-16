#include "power_manager.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include <cinttypes>
#include <cstring>
#include <initializer_list>

namespace home {
namespace {
struct Stats {
    uint32_t schema = 1, boot_id = 0, reset_reason = 0;
    char firmware[16] = {};
    uint64_t uptime_us = 0, sleep_attempt_us = 0, network_bytes = 0, network_ms = 0, display_ms = 0;
    uint32_t sleep_attempts = 0, requests = 0, request_failures = 0, frames = 0, frame_failures = 0;
};
Stats stats;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
esp_pm_lock_handle_t usb_lock = nullptr;
bool usb_locked = true;
bool storage_ok = true;
esp_err_t AfterSleep(int64_t us, void*) {
    if (us > 0) {
        portENTER_CRITICAL(&mux);
        stats.sleep_attempt_us += us; ++stats.sleep_attempts;
        portEXIT_CRITICAL(&mux);
    }
    return ESP_OK;
}
Stats Snapshot() {
    portENTER_CRITICAL(&mux); auto copy = stats; portEXIT_CRITICAL(&mux);
    copy.uptime_us = esp_timer_get_time(); return copy;
}
void Print(FILE* output, const char* kind, const Stats& s) {
    fprintf(output, "POWER_STATS,{\"kind\":\"%s\",\"schema\":1,\"firmware\":\"%.15s\",\"boot_id\":%" PRIu32
        ",\"reset_reason\":%" PRIu32 ",\"uptime_us\":%" PRIu64 ",\"sleep_attempt_us\":%" PRIu64
        ",\"sleep_attempts\":%" PRIu32 ",\"network_bytes\":%" PRIu64 ",\"network_ms\":%" PRIu64
        ",\"requests\":%" PRIu32 ",\"request_failures\":%" PRIu32 ",\"frames\":%" PRIu32
        ",\"frame_failures\":%" PRIu32 ",\"display_ms\":%" PRIu64 "}\n",
        kind, s.firmware, s.boot_id, s.reset_reason, s.uptime_us, s.sleep_attempt_us, s.sleep_attempts,
        s.network_bytes, s.network_ms, s.requests, s.request_failures, s.frames, s.frame_failures, s.display_ms);
}
}
esp_err_t InitPowerSaving(uint32_t boot_id) {
    stats.boot_id = boot_id; stats.reset_reason = esp_reset_reason();
    strncpy(stats.firmware, esp_app_get_description()->version, sizeof(stats.firmware) - 1);
    auto err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "usb_or_setup", &usb_lock);
    if (err != ESP_OK) return err;
    err = esp_pm_lock_acquire(usb_lock); if (err != ESP_OK) return err;
    esp_pm_sleep_cbs_register_config_t callbacks{}; callbacks.exit_cb = AfterSleep;
    err = esp_pm_light_sleep_register_cbs(&callbacks); if (err != ESP_OK) return err;
    esp_pm_config_t config{};
    config.max_freq_mhz = 160; config.min_freq_mhz = 40; config.light_sleep_enable = true;
    return esp_pm_configure(&config);
}
void SetUsbPower(bool powered) {
    if (!usb_lock || powered == usb_locked) return;
    const auto err = powered ? esp_pm_lock_acquire(usb_lock) : esp_pm_lock_release(usb_lock);
    if (err == ESP_OK) usb_locked = powered;
}
void CountNetwork(uint64_t bytes, uint32_t ms, bool success) {
    portENTER_CRITICAL(&mux);
    stats.network_bytes += bytes; stats.network_ms += ms; ++stats.requests;
    if (!success) ++stats.request_failures;
    portEXIT_CRITICAL(&mux);
}
void CountDisplay(uint32_t ms, bool success) {
    portENTER_CRITICAL(&mux);
    stats.display_ms += ms; ++stats.frames; if (!success) ++stats.frame_failures;
    portEXIT_CRITICAL(&mux);
}
esp_err_t SavePowerStats(bool external_power) {
    if (!storage_ok) return ESP_ERR_INVALID_STATE;
    nvs_handle_t h; auto err = nvs_open("power_v1", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        const auto copy = Snapshot();
        // Keep the previous boot's last saved counters after a USB reset.
        Stats previous{}; size_t size = sizeof(previous);
        if (nvs_get_blob(h, "latest", &previous, &size) == ESP_OK && size == sizeof(previous) && previous.boot_id != copy.boot_id)
            err = nvs_set_blob(h, "previous", &previous, sizeof(previous));
        if (err == ESP_OK) err = nvs_set_blob(h, "latest", &copy, sizeof(copy));
        // USB resets can rotate both latest/previous before inspection. Keep the
        // last battery-powered snapshot until another discharge sample replaces it.
        if (err == ESP_OK && !external_power) err = nvs_set_blob(h, "discharge", &copy, sizeof(copy));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK) storage_ok = false;
    return err;
}
void DumpPowerStats(FILE* output) {
    fprintf(output, "POWER_BEGIN,1\n");
    Print(output, "current", Snapshot());
    nvs_handle_t h;
    if (nvs_open("power_v1", NVS_READONLY, &h) == ESP_OK) {
        for (const char* key : {"latest", "previous", "discharge"}) {
            Stats saved{}; size_t size = sizeof(saved);
            if (nvs_get_blob(h, key, &saved, &size) == ESP_OK && size == sizeof(saved) && saved.schema == 1) Print(output, key, saved);
        }
        nvs_close(h);
    }
    fprintf(output, "POWER_END,1\n"); fflush(output);
}
}
