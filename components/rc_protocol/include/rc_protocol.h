#pragma once

#include "rc_core.h"
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define RC_PROTOCOL_VERSION 1

typedef enum {
    RC_PACKET_TYPE_RC        = 0x01,
    RC_PACKET_TYPE_TELEMETRY = 0x02,
    RC_PACKET_TYPE_BIND      = 0x03,
    RC_PACKET_TYPE_OTA       = 0x04,
} rc_packet_type_t;

typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t sequence;
    uint8_t  channel_count;
    int16_t  channels[RC_MAX_CHANNELS];
} rc_packet_t;
