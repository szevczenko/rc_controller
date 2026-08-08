#include "input_switch.h"
#include "esp_log.h"

static const char *TAG = "input_switch";

static gpio_num_t s_pins[INPUT_SWITCH_MAX];
static size_t     s_count;

esp_err_t input_switch_init(const gpio_num_t *pins, size_t count)
{
    if (!pins || count == 0 || count > INPUT_SWITCH_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    s_count = count;
    for (size_t i = 0; i < count; i++) {
        s_pins[i] = pins[i];
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << pins[i]),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_config pin %d: %s", pins[i], esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "switch[%zu] gpio=%d (pull-up)", i, pins[i]);
    }
    return ESP_OK;
}

bool input_switch_get(uint8_t index)
{
    if (index >= s_count) {
        return false;
    }
    /* Active-low: switch closed = GND = 0 → ON */
    return gpio_get_level(s_pins[index]) == 0;
}
