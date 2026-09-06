#pragma once
#include <cstdint>
#include <cstdio>
#include "nvs.h"

namespace home {
// Persisted v1 format. Voltage is the board's calibrated, averaged ADC reading.
struct BatteryRecord {
    int64_t unix_time = 0; // Zero until network time is valid.
    uint32_t sequence = 0;
    uint32_t boot_id = 0;
    uint32_t uptime_s = 0;
    uint16_t millivolts = 0;
    uint8_t percent = 0;
    uint8_t flags = 0;
};
static_assert(sizeof(BatteryRecord) == 24);
enum BatteryFlags : uint8_t {
    kBatteryValid = 1, kExternalPower = 2, kCharging = 4, kFull = 8,
    kFault = 16, kNoBattery = 32, kWifiConnected = 64,
};

// Uses only its own NVS namespace. Errors stop writes until the next boot;
// settings are never erased to recover logging space.
class BatteryHistory {
public:
    static constexpr unsigned kCapacity = 512;
    esp_err_t Init();
    esp_err_t Append(BatteryRecord record);
    esp_err_t DumpCsv(FILE* output) const;
private:
    static constexpr unsigned kBlockSize = 32;
    struct Block { BatteryRecord records[kBlockSize] = {}; };
    esp_err_t ReadBlock(unsigned index, Block& block) const;
    static bool Valid(const BatteryRecord& record, unsigned slot);
    nvs_handle_t handle_ = 0;
    uint32_t last_sequence_ = 0;
    bool writable_ = false;
};
}
