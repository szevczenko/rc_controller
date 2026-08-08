#pragma once

#include <stddef.h>
#include "esp_err.h"

esp_err_t config_load(const char *namespace_, const char *key, void *out, size_t size);
esp_err_t config_save(const char *namespace_, const char *key, const void *data, size_t size);
esp_err_t config_reset(const char *namespace_);
