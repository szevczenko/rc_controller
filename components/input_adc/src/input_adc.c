#include "input_adc.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define CAL_NAMESPACE "input_cal"

static const char *TAG = "input_adc";

static struct {
    adc_oneshot_unit_handle_t handles[2]; /* index = adc_unit_t (0 = UNIT_1, 1 = UNIT_2) */
    input_adc_config_t        channels[INPUT_ADC_MAX_CHANNELS];
    size_t                    count;
} s_ctx;

esp_err_t input_adc_init(const input_adc_config_t *config, size_t count)
{
    if (!config || count == 0 || count > INPUT_ADC_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    memcpy(s_ctx.channels, config, count * sizeof(input_adc_config_t));
    s_ctx.count = count;

    for (size_t i = 0; i < count; i++) {
        adc_unit_t unit = config[i].unit;
        uint8_t unit_idx = (unit == ADC_UNIT_1) ? 0 : 1;

        if (!s_ctx.handles[unit_idx]) {
            adc_oneshot_unit_init_cfg_t init_cfg = {.unit_id = unit};
            esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_ctx.handles[unit_idx]);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "failed to init ADC unit %d: %s", unit, esp_err_to_name(err));
                return err;
            }
        }

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten    = config[i].atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t err = adc_oneshot_config_channel(s_ctx.handles[unit_idx],
                                                   config[i].channel, &chan_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to config channel %d: %s", config[i].channel,
                     esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "channel[%zu] unit=%d ch=%d atten=%d", i, unit,
                 config[i].channel, config[i].atten);
    }
    return ESP_OK;
}

uint16_t input_adc_read_raw(uint8_t index)
{
    if (index >= s_ctx.count) {
        return 0;
    }
    adc_unit_t unit     = s_ctx.channels[index].unit;
    uint8_t    unit_idx = (unit == ADC_UNIT_1) ? 0 : 1;

    if (!s_ctx.handles[unit_idx]) {
        return 0;
    }

    int raw = 0;
    adc_oneshot_read(s_ctx.handles[unit_idx], s_ctx.channels[index].channel, &raw);
    return (uint16_t)raw;
}

esp_err_t input_adc_deinit(void)
{
    for (int i = 0; i < 2; i++) {
        if (s_ctx.handles[i]) {
            adc_oneshot_del_unit(s_ctx.handles[i]);
            s_ctx.handles[i] = NULL;
        }
    }
    s_ctx.count = 0;
    return ESP_OK;
}

esp_err_t input_calibration_load(uint8_t channel, input_calibration_t *cal)
{
    if (!cal) {
        return ESP_ERR_INVALID_ARG;
    }
    char key[8];
    snprintf(key, sizeof(key), "ch_%u", channel);
    esp_err_t err = config_load(CAL_NAMESPACE, key, cal, sizeof(*cal));
    if (err != ESP_OK) {
        /* Default: full ADC range, center at mid-point. */
        cal->min    = 0;
        cal->center = 2048;
        cal->max    = 4095;
        ESP_LOGW(TAG, "cal ch%u not found, using defaults", channel);
    }
    return ESP_OK;
}

esp_err_t input_calibration_save(uint8_t channel, const input_calibration_t *cal)
{
    if (!cal) {
        return ESP_ERR_INVALID_ARG;
    }
    char key[8];
    snprintf(key, sizeof(key), "ch_%u", channel);
    return config_save(CAL_NAMESPACE, key, cal, sizeof(*cal));
}
