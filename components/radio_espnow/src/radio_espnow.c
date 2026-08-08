#include "radio_espnow.h"
#include "config.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define ESPNOW_CHANNEL      1
#define BIND_NAMESPACE      "radio"
#define BIND_KEY_PEER_MAC   "peer_mac"

static const char *TAG = "radio_espnow";

static radio_rx_callback_t s_rx_cb;
static uint8_t             s_peer_mac[6];
static bool                s_bound;
static SemaphoreHandle_t   s_bind_sem;

/* ── ESP-NOW callbacks ──────────────────────────────────────────────────── */

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len <= 0) {
        return;
    }
    if (s_rx_cb) {
        s_rx_cb(data, (size_t)len, info->rx_ctrl->rssi);
    }
}

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "send failed to " MACSTR, MAC2STR(tx_info->des_addr));
    }
}

/* ── radio_driver_t implementation ─────────────────────────────────────── */

static esp_err_t espnow_init(radio_rx_callback_t rx_cb)
{
    s_rx_cb = rx_cb;

    /* Wi-Fi must be started in station mode for ESP-NOW. */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    err = esp_wifi_start();
    if (err != ESP_OK) return err;

    err = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) return err;

    err = esp_now_init();
    if (err != ESP_OK) return err;

    esp_now_register_recv_cb(espnow_recv_cb);
    esp_now_register_send_cb(espnow_send_cb);

    /* Try to restore a previously bound peer. */
    if (radio_espnow_load_peer(s_peer_mac) == ESP_OK) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, s_peer_mac, 6);
        peer.channel = ESPNOW_CHANNEL;
        peer.ifidx   = ESP_IF_WIFI_STA;
        esp_now_add_peer(&peer);
        s_bound = true;
        ESP_LOGI(TAG, "restored peer " MACSTR, MAC2STR(s_peer_mac));
    }
    ESP_LOGI(TAG, "ESP-NOW ready, bound=%d", s_bound);
    return ESP_OK;
}

static esp_err_t espnow_send(const uint8_t *data, size_t len)
{
    if (!s_bound) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_now_send(s_peer_mac, data, len);
}

static esp_err_t espnow_set_channel(uint8_t channel)
{
    return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

static int espnow_get_rssi(void)
{
    return 0; /* RSSI provided per-packet in rx_cb */
}

static const radio_driver_t s_espnow_driver = {
    .init        = espnow_init,
    .send        = espnow_send,
    .set_channel = espnow_set_channel,
    .get_rssi    = espnow_get_rssi,
    .name        = "espnow",
};

const radio_driver_t *radio_espnow_get_driver(void)
{
    return &s_espnow_driver;
}

/* ── Binding (MR-17.5) ──────────────────────────────────────────────────── */

#define BIND_MAGIC_TX  0xB1
#define BIND_MAGIC_RX  0xB2

static void bind_rx_cb(const uint8_t *data, size_t len, int rssi)
{
    (void)rssi;
    if (len < 1) return;

    if (data[0] == BIND_MAGIC_TX && !s_bound) {
        /* RX side received bind request from TX. */
        if (s_bind_sem) {
            xSemaphoreGiveFromISR(s_bind_sem, NULL);
        }
    } else if (data[0] == BIND_MAGIC_RX && !s_bound) {
        /* TX side received bind response from RX. */
        if (s_bind_sem) {
            xSemaphoreGiveFromISR(s_bind_sem, NULL);
        }
    }
}

esp_err_t radio_espnow_start_bind(uint32_t timeout_ms)
{
    s_bind_sem = xSemaphoreCreateBinary();
    radio_rx_callback_t prev_cb = s_rx_cb;
    s_rx_cb = bind_rx_cb;

    /* Use broadcast address for bind. */
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, broadcast, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = ESP_IF_WIFI_STA;
    if (!esp_now_is_peer_exist(broadcast)) {
        esp_now_add_peer(&peer);
    }

    uint8_t bind_pkt = BIND_MAGIC_TX;
    esp_now_send(broadcast, &bind_pkt, 1);
    ESP_LOGI(TAG, "TX: binding broadcast sent, waiting %lu ms", (unsigned long)timeout_ms);

    esp_err_t err = ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(s_bind_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        s_bound = true;
        err = ESP_OK;
        ESP_LOGI(TAG, "TX: bind successful");
    }

    s_rx_cb = prev_cb;
    vSemaphoreDelete(s_bind_sem);
    s_bind_sem = NULL;
    return err;
}

esp_err_t radio_espnow_wait_bind(uint32_t timeout_ms)
{
    s_bind_sem = xSemaphoreCreateBinary();
    radio_rx_callback_t prev_cb = s_rx_cb;
    s_rx_cb = bind_rx_cb;

    ESP_LOGI(TAG, "RX: waiting for bind request, timeout %lu ms", (unsigned long)timeout_ms);

    esp_err_t err = ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(s_bind_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        /* Respond to TX broadcast. */
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        uint8_t bind_pkt = BIND_MAGIC_RX;
        esp_now_send(broadcast, &bind_pkt, 1);
        s_bound = true;
        err = ESP_OK;
        ESP_LOGI(TAG, "RX: bind response sent");
    }

    s_rx_cb = prev_cb;
    vSemaphoreDelete(s_bind_sem);
    s_bind_sem = NULL;
    return err;
}

bool radio_espnow_is_bound(void)
{
    return s_bound;
}

esp_err_t radio_espnow_save_peer(const uint8_t peer_mac[6])
{
    memcpy(s_peer_mac, peer_mac, 6);
    return config_save(BIND_NAMESPACE, BIND_KEY_PEER_MAC, peer_mac, 6);
}

esp_err_t radio_espnow_load_peer(uint8_t peer_mac[6])
{
    return config_load(BIND_NAMESPACE, BIND_KEY_PEER_MAC, peer_mac, 6);
}
