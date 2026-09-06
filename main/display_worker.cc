#include "display_worker.h"
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"

namespace home {
esp_err_t DisplayWorker::Init(zectrix_epd_handle_t panel) {
    panel_ = panel;
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) return ESP_ERR_NO_MEM;
    if (xTaskCreate(RunTask, "home_display", 4096, this, 3, &task_) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
uint32_t DisplayWorker::Submit(const uint8_t* frame) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (requested_ && std::memcmp(pending_, frame, sizeof(pending_)) == 0) {
        const auto ticket = requested_; xSemaphoreGive(mutex_); return ticket;
    }
    memcpy(pending_, frame, sizeof(pending_));
    const auto ticket = ++requested_;
    xSemaphoreGive(mutex_);
    xTaskNotifyGive(task_);
    return ticket;
}
void DisplayWorker::Wait(uint32_t ticket) {
    while (finished_.load() < ticket) vTaskDelay(pdMS_TO_TICKS(10));
}
void DisplayWorker::RunTask(void* self) { static_cast<DisplayWorker*>(self)->Run(); }
void DisplayWorker::Run() {
    uint32_t drawn = 0;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const uint32_t ticket = requested_;
        memcpy(drawing_, pending_, sizeof(drawing_));
        xSemaphoreGive(mutex_);
        if (ticket == drawn) continue;
        const auto start = esp_timer_get_time();
        auto err = zectrix_epd_power_on(panel_);
        // Full frames keep text clean between entirely different pages. No
        // additional loading waveform; cached content is the next frame.
        if (err == ESP_OK) err = zectrix_epd_refresh_full_1bpp(panel_, drawing_, sizeof(drawing_));
        const auto off = zectrix_epd_power_off(panel_);
        if (err == ESP_OK) err = off;
        ESP_LOGI("home_display", "frame %lu: %lld ms, %s", static_cast<unsigned long>(ticket),
                 (esp_timer_get_time() - start) / 1000, esp_err_to_name(err));
        drawn = ticket;
        finished_ = ticket;
    }
}
}
