#include "app_events.h"
#include <atomic>
namespace home {
namespace { std::atomic<TaskHandle_t> app{nullptr}; }
void RegisterAppTask(TaskHandle_t task) { app = task; }
void WakeApp() { if (auto task = app.load()) xTaskNotifyGive(task); }
}
