#pragma once
#include "esp_err.h"
#include "display_metadata.h"
#include <cstdint>
#include <cstdio>
namespace home {
esp_err_t InitPowerSaving(uint32_t boot_id);
void SetUsbPower(bool powered);
void CountNetwork(uint64_t bytes, uint32_t elapsed_ms, bool success);
void StartDisplayAttempt();
void CountDisplay(uint32_t elapsed_ms, bool success, const DisplayMetadata& metadata);
esp_err_t SavePowerStats(bool external_power);
void DumpPowerStats(FILE* output);
}
