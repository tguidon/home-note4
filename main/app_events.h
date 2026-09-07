#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace home {
// Register once before starting network/feed workers.
void RegisterAppTask(TaskHandle_t task);
void WakeApp();
}
