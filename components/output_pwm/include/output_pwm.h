#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stddef.h>
#include <stdint.h>

#define OUTPUT_PWM_MAX_CHANNELS 8

typedef struct {
    gpio_num_t pin;
    uint32_t   freq_hz;  /* e.g. 50 for standard RC servo/ESC */
    uint16_t   min_us;   /* pulse width at -1000, e.g. 1000 */
    uint16_t   max_us;   /* pulse width at +1000, e.g. 2000 */
} output_pwm_config_t;

esp_err_t output_pwm_init(const output_pwm_config_t *config, size_t count);

/* value: -1000 … +1000. Clamped if out of range. */
esp_err_t output_pwm_set(uint8_t index, int16_t value);

esp_err_t output_pwm_deinit(void);
