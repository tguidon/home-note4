#include "power_manager.h"
#include "nvs.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int64_t test_time = 100;
namespace {
std::map<std::string, std::vector<unsigned char>> blobs;
// The on-device v0.6.0 layout, intentionally independent of the new structure.
struct LegacyStats {
    uint32_t schema = 1, boot_id = 42, reset_reason = 11;
    char firmware[16] = "0.6.0";
    uint64_t uptime_us = 123456, sleep_attempt_us = 1, network_bytes = 2, network_ms = 3, display_ms = 4;
    uint32_t sleep_attempts = 5, requests = 6, request_failures = 7, frames = 8, frame_failures = 9;
};
std::string Export() {
    char* bytes = nullptr; size_t size = 0;
    FILE* out = open_memstream(&bytes, &size); assert(out);
    home::DumpPowerStats(out); fclose(out);
    std::string result(bytes, size); free(bytes); return result;
}
}
esp_err_t nvs_open(const char* name, int, nvs_handle_t* h) {
    assert(std::string(name) == "power_v1"); *h = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* data, size_t* size) {
    const auto it = blobs.find(key); if (it == blobs.end()) return ESP_ERR_NVS_NOT_FOUND;
    if (data && *size < it->second.size()) return ESP_ERR_INVALID_SIZE;
    *size = it->second.size(); if (data) memcpy(data, it->second.data(), *size); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* data, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    blobs[key] = std::vector<unsigned char>(bytes, bytes + size); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
void nvs_close(nvs_handle_t) {}

int main() {
    LegacyStats old;
    nvs_set_blob(1, "latest", &old, sizeof(old));
    nvs_set_blob(1, "discharge", &old, sizeof(old));
    assert(home::InitPowerSaving(100) == ESP_OK);
    auto output = Export();
    assert(output.find("\"kind\":\"discharge\",\"schema\":1,\"firmware\":\"0.6.0\",\"boot_id\":42") != std::string::npos);
    assert(output.find("\"frames\":8,\"frame_failures\":9,\"display_ms\":4,\"frame_started_us\":0,\"last_frame_us\":0,\"last_frame_mv\":-1,\"last_frame_label\":\"\"") != std::string::npos);
    home::StartDisplayAttempt();
    home::DisplayMetadata full{4175, "100% FULL"};
    test_time = 200; home::CountDisplay(100, true, full);
    test_time = 300; home::StartDisplayAttempt();
    test_time = 400; home::CountDisplay(100, false, {3720, "~65%"});
    output = Export();
    assert(output.find("\"frames\":2,\"frame_failures\":1,\"display_ms\":200,\"frame_started_us\":300,\"last_frame_us\":200,\"last_frame_mv\":4175,\"last_frame_label\":\"100% FULL\"") != std::string::npos);
    assert(home::SavePowerStats(true) == ESP_OK);
    assert(blobs["discharge"].size() == sizeof(old)); // USB preserves the legacy discharge evidence.
    output = Export();
    assert(output.find("\"kind\":\"previous\",\"schema\":1,\"firmware\":\"0.6.0\",\"boot_id\":42") != std::string::npos);
    blobs["previous"].resize(3); // Unsupported sizes are ignored, never reinterpreted.
    assert(Export().find("\"kind\":\"previous\"") == std::string::npos);
    std::cout << "PASS: legacy power snapshots, USB preservation, successful-frame metadata and failed-frame diagnostics\n";
}
