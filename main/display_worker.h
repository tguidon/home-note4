#pragma once
#include <atomic>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "zectrix_epd.h"

namespace home {
// One pending frame: rapid navigation replaces obsolete work while the panel
// finishes its current waveform. Only this task owns the panel after Init().
class DisplayWorker {
public:
    esp_err_t Init(zectrix_epd_handle_t panel);
    uint32_t Submit(const uint8_t* frame);
    void Wait(uint32_t ticket);
private:
    static void RunTask(void* self);
    void Run();
    zectrix_epd_handle_t panel_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    uint8_t pending_[ZECTRIX_EPD_1BPP_FRAME_BYTES] = {};
    uint8_t drawing_[ZECTRIX_EPD_1BPP_FRAME_BYTES] = {};
    uint32_t requested_ = 0;
    std::atomic<uint32_t> finished_{0};
};
}
