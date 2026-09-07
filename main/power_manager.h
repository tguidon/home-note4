#pragma once
#include "esp_err.h"
#include <cstdint>
#include <cstdio>
namespace home {
esp_err_t InitPowerSaving(uint32_t boot_id);
void SetUsbPower(bool powered);
void CountNetwork(uint64_t bytes, uint32_t elapsed_ms, bool success);
void CountDisplay(uint32_t elapsed_ms, bool success);
esp_err_t SavePowerStats();
void DumpPowerStats(FILE* output);
}
