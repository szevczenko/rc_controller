#pragma once

#include "esp_err.h"
#include "hal/adc_types.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_ADC_MAX_CHANNELS 8

typedef struct {
    adc_unit_t    unit;
    adc_channel_t channel;
    adc_atten_t   atten;
} input_adc_config_t;

/* Raw ADC calibration — min/center/max in ADC counts (0–4095). */
typedef struct {
    uint16_t min;
    uint16_t center;
    uint16_t max;
} input_calibration_t;

esp_err_t input_adc_init(const input_adc_config_t *config, size_t count);
uint16_t  input_adc_read_raw(uint8_t index);
esp_err_t input_adc_deinit(void);

/* Calibration persistence — stored in NVS namespace "input_cal". */
esp_err_t input_calibration_load(uint8_t channel, input_calibration_t *cal);
esp_err_t input_calibration_save(uint8_t channel, const input_calibration_t *cal);
