#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rc_core.h"
#include "rc_protocol.h"
#include "radio.h"
#include "radio_espnow.h"
#include "input_adc.h"
#include "input_switch.h"

static const char *TAG = "tx_main";

/* ── Pin configuration — adjust to your hardware ───────────────────────── */
#define THROTTLE_ADC_UNIT    ADC_UNIT_1
#define THROTTLE_ADC_CHANNEL ADC_CHANNEL_0  /* GPIO1 on ESP32-S3 */
#define STEERING_ADC_UNIT    ADC_UNIT_1
#define STEERING_ADC_CHANNEL ADC_CHANNEL_1  /* GPIO2 on ESP32-S3 */

#define SW1_GPIO  GPIO_NUM_4
#define SW2_GPIO  GPIO_NUM_5

/* CH mapping */
#define CH_THROTTLE  0
#define CH_STEERING  1
#define CH_SW1       2
#define CH_SW2       3
#define CH_COUNT     4

#define PACKET_RATE_MS  20  /* 50 Hz */

static uint16_t s_seq;
static rc_link_stats_t s_stats;

static void radio_rx_cb(const uint8_t *data, size_t len, int rssi)
{
    /* RX → TX telemetry */
    rc_telemetry_packet_t telem;
    if (rc_telemetry_decode(data, len, &telem) == ESP_OK) {
        ESP_LOGI(TAG, "telem: rssi=%d lq=%d batt=%dmV", telem.rssi,
                 telem.link_quality, telem.battery_mv);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "TX starting");

    /* NVS init */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ADC init — throttle + steering */
    input_adc_config_t adc_cfg[] = {
        {THROTTLE_ADC_UNIT, THROTTLE_ADC_CHANNEL, ADC_ATTEN_DB_12},
        {STEERING_ADC_UNIT, STEERING_ADC_CHANNEL, ADC_ATTEN_DB_12},
    };
    ESP_ERROR_CHECK(input_adc_init(adc_cfg, 2));

    /* Switch init */
    gpio_num_t sw_pins[] = {SW1_GPIO, SW2_GPIO};
    ESP_ERROR_CHECK(input_switch_init(sw_pins, 2));

    /* Load calibration (defaults if not yet saved) */
    input_calibration_t cal[2];
    input_calibration_load(CH_THROTTLE, &cal[CH_THROTTLE]);
    input_calibration_load(CH_STEERING, &cal[CH_STEERING]);

    /* Failsafe: throttle = 0, steering = 0, switches = off */
    rc_failsafe_config_t fs_cfg = {
        .timeout_ms = 500,
        .failsafe_values = {0},
    };
    rc_core_init(&fs_cfg);

    /* Radio */
    ESP_ERROR_CHECK(radio_init(radio_espnow_get_driver(), radio_rx_cb));
    if (!radio_espnow_is_bound()) {
        ESP_LOGI(TAG, "not bound — starting bind");
        err = radio_espnow_start_bind(5000);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "bind failed, running unbound");
        }
    }

    rc_calibration_t rc_cal[2];
    for (int i = 0; i < 2; i++) {
        rc_cal[i].min    = cal[i].min;
        rc_cal[i].center = cal[i].center;
        rc_cal[i].max    = cal[i].max;
    }

    rc_core_channel_config_t dz_cfg = {.deadzone = 30};
    /* expo 20 = mild curve, 0 = linear; load from NVS in future MR */
    const uint8_t expo = 20;

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        uint16_t thr_raw = input_adc_read_raw(CH_THROTTLE);
        uint16_t str_raw = input_adc_read_raw(CH_STEERING);

        int16_t thr = rc_core_apply_expo(
                          rc_core_apply_deadzone(rc_core_normalize(thr_raw, &rc_cal[0]), &dz_cfg),
                          expo);
        int16_t str = rc_core_apply_expo(
                          rc_core_apply_deadzone(rc_core_normalize(str_raw, &rc_cal[1]), &dz_cfg),
                          expo);

        rc_channel_state_t ch_state = {
            .channel_count = CH_COUNT,
            .channels = {
                [CH_THROTTLE] = thr,
                [CH_STEERING] = str,
                [CH_SW1]      = input_switch_get(0) ? 1000 : -1000,
                [CH_SW2]      = input_switch_get(1) ? 1000 : -1000,
            },
        };

        rc_packet_t pkt = {
            .version       = RC_PROTOCOL_VERSION,
            .type          = RC_PACKET_TYPE_RC,
            .sequence      = s_seq++,
            .channel_count = CH_COUNT,
        };
        for (int i = 0; i < CH_COUNT; i++) {
            pkt.channels[i] = ch_state.channels[i];
        }

        uint8_t buf[RC_PACKET_MAX_SIZE];
        int len = rc_packet_encode(&pkt, buf, sizeof(buf));
        if (len > 0) {
            radio_send(buf, (size_t)len);
        }

        rc_core_tick(now_ms);

        vTaskDelay(pdMS_TO_TICKS(PACKET_RATE_MS));
    }
}

