#pragma once

#include <stdint.h>

#define RC_MAX_CHANNELS 16

typedef struct {
    int16_t channels[RC_MAX_CHANNELS];
    uint8_t channel_count;
} rc_channel_state_t;
