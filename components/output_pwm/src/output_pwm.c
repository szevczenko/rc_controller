#include "output_pwm.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <string.h>

#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION  LEDC_TIMER_14_BIT   /* 16384 ticks */

static const char *TAG = "output_pwm";

static struct {
    output_pwm_config_t cfg[OUTPUT_PWM_MAX_CHANNELS];
    size_t              count;
    uint32_t            period_ticks; /* full period in LEDC ticks */
} s_ctx;

static uint32_t us_to_ticks(uint32_t us, uint32_t freq_hz)
{
    /* ticks = (us / 1_000_000) * freq_hz * (1 << LEDC_RESOLUTION) */
    return (us * freq_hz * (1u << LEDC_RESOLUTION)) / 1000000u;
}

esp_err_t output_pwm_init(const output_pwm_config_t *config, size_t count)
{
    if (!config || count == 0 || count > OUTPUT_PWM_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    memcpy(s_ctx.cfg, config, count * sizeof(output_pwm_config_t));
    s_ctx.count = count;

    /* All channels must use the same frequency (single shared timer). */
    uint32_t freq = config[0].freq_hz;
    for (size_t i = 1; i < count; i++) {
        if (config[i].freq_hz != freq) {
            ESP_LOGE(TAG, "all channels must share freq, ch[%zu]=%lu != %lu",
                     i, (unsigned long)config[i].freq_hz, (unsigned long)freq);
            return ESP_ERR_INVALID_ARG;
        }
    }

    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = freq,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc timer config: %s", esp_err_to_name(err));
        return err;
    }

    for (size_t i = 0; i < count; i++) {
        ledc_channel_config_t ch = {
            .gpio_num   = config[i].pin,
            .speed_mode = LEDC_MODE,
            .channel    = (ledc_channel_t)i,
            .timer_sel  = LEDC_TIMER,
            .duty       = us_to_ticks(config[i].min_us, freq),
            .hpoint     = 0,
        };
        err = ledc_channel_config(&ch);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc channel %zu: %s", i, esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "pwm[%zu] pin=%d freq=%luHz min=%uus max=%uus",
                 i, config[i].pin, (unsigned long)config[i].freq_hz,
                 config[i].min_us, config[i].max_us);
    }
    return ESP_OK;
}

esp_err_t output_pwm_set(uint8_t index, int16_t value)
{
    if (index >= s_ctx.count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (value < -1000) value = -1000;
    if (value >  1000) value =  1000;

    const output_pwm_config_t *cfg = &s_ctx.cfg[index];
    uint32_t center_us = (cfg->min_us + cfg->max_us) / 2;
    uint32_t half_range = (cfg->max_us - cfg->min_us) / 2;
    uint32_t pulse_us   = (uint32_t)(center_us + (int32_t)value * (int32_t)half_range / 1000);

    uint32_t duty = us_to_ticks(pulse_us, cfg->freq_hz);
    return ledc_set_duty_and_update(LEDC_MODE, (ledc_channel_t)index, duty, 0);
}

esp_err_t output_pwm_deinit(void)
{
    for (size_t i = 0; i < s_ctx.count; i++) {
        ledc_stop(LEDC_MODE, (ledc_channel_t)i, 0);
    }
    s_ctx.count = 0;
    return ESP_OK;
}
