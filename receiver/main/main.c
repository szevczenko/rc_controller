#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rc_core.h"
#include "rc_protocol.h"
#include "radio.h"
#include "radio_espnow.h"
#include "output_pwm.h"

static const char *TAG = "rx_main";

/* ── Pin configuration — adjust to your hardware ───────────────────────── */
#define ESC_GPIO    GPIO_NUM_18
#define SERVO_GPIO  GPIO_NUM_19

#define CH_THROTTLE  0
#define CH_STEERING  1
#define CH_SW_ARM    2

static rc_link_stats_t s_stats;
static rc_arm_state_t  s_arm;
static uint16_t        s_last_seq;
static uint32_t        s_uptime_ms;

static void radio_rx_cb(const uint8_t *data, size_t len, int rssi)
{
    rc_packet_t pkt;
    if (rc_packet_decode(data, len, &pkt) != ESP_OK) {
        rc_link_stats_update(&s_stats, rssi, true, s_last_seq);
        return;
    }

    rc_seq_result_t seq_res = rc_protocol_check_sequence(pkt.sequence, &s_last_seq);
    bool lost = (seq_res != RC_SEQ_OK);
    rc_link_stats_update(&s_stats, rssi, lost, pkt.sequence);

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    rc_core_packet_received(now_ms);

    rc_channel_state_t ch = {
        .channel_count = pkt.channel_count,
    };
    for (int i = 0; i < pkt.channel_count; i++) {
        ch.channels[i] = pkt.channels[i];
    }

    rc_arm_update(&s_arm, &ch, CH_SW_ARM, CH_THROTTLE);

    int16_t throttle = s_arm.armed ? ch.channels[CH_THROTTLE] : 0;
    int16_t steering = ch.channels[CH_STEERING];

    output_pwm_set(0, throttle);  /* ESC */
    output_pwm_set(1, steering);  /* servo */

    /* Send telemetry back to TX */
    rc_telemetry_packet_t telem = {
        .version       = RC_PROTOCOL_VERSION,
        .type          = RC_PACKET_TYPE_TELEMETRY,
        .rssi          = (int8_t)rssi,
        .link_quality  = s_stats.link_quality,
        .battery_mv    = 0,  /* placeholder — connect ADC to battery divider */
        .uptime_ms     = s_uptime_ms,
    };
    uint8_t telem_buf[RC_TELEMETRY_PACKET_SIZE];
    int tlen = rc_telemetry_encode(&telem, telem_buf, sizeof(telem_buf));
    if (tlen > 0) {
        radio_send(telem_buf, (size_t)tlen);
    }
}

static void failsafe_apply(void)
{
    output_pwm_set(0, 0);  /* ESC safe */
    output_pwm_set(1, 0);  /* servo center */
    s_arm.armed = false;
    ESP_LOGW(TAG, "FAILSAFE");
}

void app_main(void)
{
    ESP_LOGI(TAG, "RX starting");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* PWM: ESC (CH0) + servo (CH1) at 50 Hz, 1000–2000 µs */
    output_pwm_config_t pwm_cfg[] = {
        {ESC_GPIO,   50, 1000, 2000},
        {SERVO_GPIO, 50, 1000, 2000},
    };
    ESP_ERROR_CHECK(output_pwm_init(pwm_cfg, 2));

    /* Failsafe: throttle = 0, steering = 0 */
    rc_failsafe_config_t fs_cfg = {
        .timeout_ms = 500,
        .failsafe_values = {0},
    };
    rc_core_init(&fs_cfg);

    /* Radio */
    ESP_ERROR_CHECK(radio_init(radio_espnow_get_driver(), radio_rx_cb));
    if (!radio_espnow_is_bound()) {
        ESP_LOGI(TAG, "not bound — waiting for bind");
        err = radio_espnow_wait_bind(5000);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "bind timeout, running unbound");
        }
    }

    /* Uptime counter + failsafe tick at 10 Hz */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_uptime_ms += 100;
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        rc_core_tick(now_ms);
        if (rc_core_get_link_state() == RC_STATE_FAILSAFE) {
            failsafe_apply();
        }
    }
}

