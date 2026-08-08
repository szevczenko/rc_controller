#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef void (*radio_rx_callback_t)(const uint8_t *data, size_t len, int rssi);

typedef struct {
    esp_err_t   (*init)(radio_rx_callback_t rx_cb);
    esp_err_t   (*send)(const uint8_t *data, size_t len);
    esp_err_t   (*set_channel)(uint8_t channel);
    int         (*get_rssi)(void);
    const char  *name;
} radio_driver_t;

esp_err_t radio_init(const radio_driver_t *driver, radio_rx_callback_t rx_cb);
esp_err_t radio_send(const uint8_t *data, size_t len);
esp_err_t radio_set_channel(uint8_t channel);
int       radio_get_rssi(void);

/* Loopback driver: send() immediately fires rx_cb with the same data. */
const radio_driver_t *radio_loopback_get_driver(void);
