#include "radio.h"
#include "esp_log.h"

static const char *TAG = "radio";

static const radio_driver_t *s_driver;

esp_err_t radio_init(const radio_driver_t *driver, radio_rx_callback_t rx_cb)
{
    if (!driver || !driver->init) {
        return ESP_ERR_INVALID_ARG;
    }
    s_driver = driver;
    ESP_LOGI(TAG, "init driver: %s", driver->name ? driver->name : "?");
    return driver->init(rx_cb);
}

esp_err_t radio_send(const uint8_t *data, size_t len)
{
    if (!s_driver || !s_driver->send) {
        return ESP_ERR_INVALID_STATE;
    }
    return s_driver->send(data, len);
}
