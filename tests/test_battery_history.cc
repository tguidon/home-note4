#include "battery_history.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
std::map<std::string, std::vector<unsigned char>> stored, pending;
bool fail_write = false, fail_commit = false;
unsigned writes = 0;
std::string Export(const home::BatteryHistory& history) {
    char* bytes = nullptr; size_t size = 0;
    FILE* stream = open_memstream(&bytes, &size);
    assert(stream);
    assert(history.DumpCsv(stream) == ESP_OK);
    fclose(stream);
    std::string result(bytes, size); free(bytes);
    return result;
}
}
esp_err_t nvs_open(const char* name, int, nvs_handle_t* handle) {
    assert(std::string(name) == "battery_v1"); *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* data, size_t* size) {
    auto it = stored.find(key);
    if (it == stored.end()) return ESP_ERR_NVS_NOT_FOUND;
    if (*size < it->second.size()) return ESP_ERR_INVALID_SIZE;
    *size = it->second.size(); memcpy(data, it->second.data(), *size); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* data, size_t size) {
    ++writes;
    if (fail_write) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    const auto* bytes = static_cast<const unsigned char*>(data);
    pending[key] = std::vector<unsigned char>(bytes, bytes + size); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) {
    if (fail_commit) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    for (const auto& entry : pending) stored[entry.first] = entry.second;
    pending.clear(); return ESP_OK;
}
int main() {
    home::BatteryHistory history;
    assert(history.Init() == ESP_OK);
    assert(Export(history).find("BATTERY_END,0\n") != std::string::npos);
    home::BatteryRecord record;
    record.boot_id = 100; record.millivolts = 4000; record.percent = 80;
    record.flags = home::kBatteryValid | home::kExternalPower | home::kCharging;
    // More than two wraps; persisted capacity and chronological export stay bounded.
    for (unsigned i = 1; i <= 1100; ++i) {
        record.uptime_s = i * 900; record.unix_time = 1800000000 + record.uptime_s;
        assert(history.Append(record) == ESP_OK);
    }
    assert(stored.size() == 16);
    size_t bytes = 0; for (const auto& entry : stored) bytes += entry.second.size();
    assert(bytes == 12288);
    auto csv = Export(history);
    assert(csv.find("BATTERY_ROW,588,") == std::string::npos);
    assert(csv.find("BATTERY_ROW,589,100,") != std::string::npos);
    assert(csv.find("BATTERY_ROW,1100,100,") != std::string::npos);
    assert(csv.find("BATTERY_END,512\n") != std::string::npos);
    // Restart discovers the committed head; new sessions retain earlier data.
    home::BatteryHistory restarted;
    assert(restarted.Init() == ESP_OK);
    assert(Export(restarted) == csv);
    record.boot_id = 200; record.uptime_s = 10; record.unix_time = 0;
    record.flags = home::kBatteryValid;
    assert(restarted.Append(record) == ESP_OK);
    csv = Export(restarted);
    assert(csv.find("BATTERY_ROW,1101,200,0,10,4000,80,1,0,0,0,0,0,0\n") != std::string::npos);
    // Full storage cannot trigger erase, overwrite other namespaces or a retry storm.
    const auto before_failure = stored;
    fail_write = true;
    assert(restarted.Append(record) == ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    const unsigned stopped_at = writes;
    assert(restarted.Append(record) == ESP_ERR_INVALID_STATE);
    assert(writes == stopped_at && stored == before_failure);
    fail_write = false;
    // A failed commit also leaves the previous committed history readable after restart.
    home::BatteryHistory after_failure; assert(after_failure.Init() == ESP_OK);
    fail_commit = true;
    assert(after_failure.Append(record) == ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    pending.clear(); fail_commit = false;
    home::BatteryHistory recovered; assert(recovered.Init() == ESP_OK);
    assert(Export(recovered) == csv);
    // A malformed stored block is rejected without erasing or modifying storage.
    stored["b00"].resize(3);
    const auto corrupt = stored;
    home::BatteryHistory bad; assert(bad.Init() == ESP_ERR_INVALID_SIZE);
    assert(bad.Append(record) == ESP_ERR_INVALID_STATE);
    assert(stored == corrupt);
    puts("Battery history tests passed (wrap, restart, storage failure, corrupt block)");
}
