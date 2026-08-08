#pragma once

#include "radio.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Returns the ESP-NOW radio_driver_t instance. */
const radio_driver_t *radio_espnow_get_driver(void);

/* ── Binding (MR-17.5) ──────────────────────────────────────────────────── */

/* TX side: broadcast BIND packet and wait for a response. */
esp_err_t radio_espnow_start_bind(uint32_t timeout_ms);

/* RX side: listen for a BIND packet and respond. */
esp_err_t radio_espnow_wait_bind(uint32_t timeout_ms);

bool radio_espnow_is_bound(void);

/* Store/load peer MAC in NVS namespace "radio". */
esp_err_t radio_espnow_save_peer(const uint8_t peer_mac[6]);
esp_err_t radio_espnow_load_peer(uint8_t peer_mac[6]);
