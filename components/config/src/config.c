#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config";

esp_err_t config_load(const char *namespace_, const char *key, void *out, size_t size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_blob(handle, key, out, &size);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "load %s/%s: %s", namespace_, key, esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_save(const char *namespace_, const char *key, const void *data, size_t size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, key, data, size);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save %s/%s: %s", namespace_, key, esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_reset(const char *namespace_)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
