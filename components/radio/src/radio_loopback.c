#include "radio.h"
#include <string.h>

static radio_rx_callback_t s_rx_cb;

static esp_err_t loopback_init(radio_rx_callback_t rx_cb)
{
    s_rx_cb = rx_cb;
    return ESP_OK;
}

static esp_err_t loopback_send(const uint8_t *data, size_t len)
{
    if (s_rx_cb) {
        s_rx_cb(data, len, 0);
    }
    return ESP_OK;
}

static esp_err_t loopback_set_channel(uint8_t channel)
{
    (void)channel;
    return ESP_OK;
}

static int loopback_get_rssi(void)
{
    return 0;
}

static const radio_driver_t s_loopback_driver = {
    .init        = loopback_init,
    .send        = loopback_send,
    .set_channel = loopback_set_channel,
    .get_rssi    = loopback_get_rssi,
    .name        = "loopback",
};

const radio_driver_t *radio_loopback_get_driver(void)
{
    return &s_loopback_driver;
}
