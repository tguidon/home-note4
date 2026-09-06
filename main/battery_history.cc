#include "battery_history.h"
#include <algorithm>
#include <cinttypes>
#include <limits>

namespace home {
namespace {
void Key(unsigned block, char* key, size_t size) { snprintf(key, size, "b%02u", block); }
}
bool BatteryHistory::Valid(const BatteryRecord& r, unsigned slot) {
    return r.sequence != 0 && (r.sequence - 1) % kCapacity == slot &&
           r.percent <= 100 && !(r.flags & 128);
}
esp_err_t BatteryHistory::ReadBlock(unsigned index, Block& block) const {
    block = {};
    char key[8]; Key(index, key, sizeof(key));
    size_t size = sizeof(block);
    auto err = nvs_get_blob(handle_, key, &block, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) { block = {}; return ESP_OK; }
    if (err == ESP_OK && size != sizeof(block)) return ESP_ERR_INVALID_SIZE;
    return err;
}
esp_err_t BatteryHistory::Init() {
    auto err = nvs_open("battery_v1", NVS_READWRITE, &handle_);
    if (err != ESP_OK) return err;
    for (unsigned b = 0; b < kCapacity / kBlockSize; ++b) {
        Block block;
        err = ReadBlock(b, block);
        if (err != ESP_OK) return err;
        for (unsigned i = 0; i < kBlockSize; ++i) {
            const auto& r = block.records[i];
            if (r.sequence && !Valid(r, b * kBlockSize + i)) return ESP_ERR_INVALID_STATE;
            last_sequence_ = std::max(last_sequence_, r.sequence);
        }
    }
    writable_ = true;
    return ESP_OK;
}
esp_err_t BatteryHistory::Append(BatteryRecord record) {
    if (!writable_ || last_sequence_ == std::numeric_limits<uint32_t>::max()) return ESP_ERR_INVALID_STATE;
    record.sequence = last_sequence_ + 1;
    const unsigned slot = (record.sequence - 1) % kCapacity;
    if (!Valid(record, slot)) return ESP_ERR_INVALID_ARG;
    Block block;
    auto err = ReadBlock(slot / kBlockSize, block);
    if (err == ESP_OK) {
        block.records[slot % kBlockSize] = record;
        char key[8]; Key(slot / kBlockSize, key, sizeof(key));
        err = nvs_set_blob(handle_, key, &block, sizeof(block));
        if (err == ESP_OK) err = nvs_commit(handle_);
    }
    if (err == ESP_OK) last_sequence_ = record.sequence;
    else writable_ = false;
    return err;
}
esp_err_t BatteryHistory::DumpCsv(FILE* output) const {
    if (!handle_) return ESP_ERR_INVALID_STATE;
    fprintf(output, "BATTERY_BEGIN,1\n");
    fprintf(output, "BATTERY_HEADER,sequence,boot_id,unix_time,uptime_s,millivolts,percent,battery_valid,external_power,charging,full,fault,no_battery,wifi_connected\n");
    const uint32_t first = last_sequence_ > kCapacity ? last_sequence_ - kCapacity + 1 : 1;
    unsigned cached_block = kCapacity, count = 0;
    Block block;
    for (uint64_t sequence = first; sequence <= last_sequence_; ++sequence) {
        const unsigned slot = (sequence - 1) % kCapacity;
        if (cached_block != slot / kBlockSize) {
            cached_block = slot / kBlockSize;
            auto err = ReadBlock(cached_block, block);
            if (err != ESP_OK) return err; // No END marker: reader rejects partial exports.
        }
        const auto& r = block.records[slot % kBlockSize];
        if (!Valid(r, slot) || r.sequence != sequence) return ESP_ERR_INVALID_STATE;
        fprintf(output, "BATTERY_ROW,%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRIu32 ",%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                r.sequence, r.boot_id, r.unix_time, r.uptime_s, r.millivolts, r.percent,
                !!(r.flags & kBatteryValid), !!(r.flags & kExternalPower), !!(r.flags & kCharging),
                !!(r.flags & kFull), !!(r.flags & kFault), !!(r.flags & kNoBattery), !!(r.flags & kWifiConnected));
        ++count;
    }
    fprintf(output, "BATTERY_END,%u\n", count);
    fflush(output);
    return ESP_OK;
}
}
