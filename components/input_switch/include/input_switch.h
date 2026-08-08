#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define INPUT_SWITCH_MAX 8

esp_err_t input_switch_init(const gpio_num_t *pins, size_t count);
bool      input_switch_get(uint8_t index);
