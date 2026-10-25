#pragma once
#include "../stubs/nvs.h"
constexpr int NVS_READONLY = 0;
void nvs_close(nvs_handle_t);
extern int64_t test_time;
inline int64_t esp_timer_get_time() { return test_time; }
inline int esp_reset_reason() { return 1; }
struct esp_app_desc_t { const char* version = "test"; };
inline const esp_app_desc_t* esp_app_get_description() { static esp_app_desc_t app; return &app; }
using esp_pm_lock_handle_t = void*;
constexpr int ESP_PM_NO_LIGHT_SLEEP = 0;
inline int esp_pm_lock_create(int, int, const char*, void** h) { *h = reinterpret_cast<void*>(1); return ESP_OK; }
inline int esp_pm_lock_acquire(void*) { return ESP_OK; }
inline int esp_pm_lock_release(void*) { return ESP_OK; }
struct esp_pm_sleep_cbs_register_config_t { esp_err_t (*exit_cb)(int64_t, void*) = nullptr; };
inline int esp_pm_light_sleep_register_cbs(esp_pm_sleep_cbs_register_config_t*) { return ESP_OK; }
struct esp_pm_config_t { int max_freq_mhz = 0, min_freq_mhz = 0; bool light_sleep_enable = false; };
inline int esp_pm_configure(esp_pm_config_t*) { return ESP_OK; }
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
