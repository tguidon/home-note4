#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using nvs_handle_t = uint32_t;
constexpr int ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 1, ESP_ERR_INVALID_SIZE = 2,
    ESP_ERR_INVALID_STATE = 3, ESP_ERR_INVALID_ARG = 4, ESP_ERR_NVS_NOT_ENOUGH_SPACE = 5;
constexpr int NVS_READWRITE = 1;
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
esp_err_t nvs_get_blob(nvs_handle_t, const char*, void*, size_t*);
esp_err_t nvs_set_blob(nvs_handle_t, const char*, const void*, size_t);
esp_err_t nvs_commit(nvs_handle_t);
