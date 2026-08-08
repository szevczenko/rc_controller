#pragma once

#include "rc_core.h"
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define RC_PROTOCOL_VERSION  1
/* wire size: header(5) + channels(RC_MAX_CHANNELS*2) + crc(2) */
#define RC_PACKET_MAX_SIZE   (5 + RC_MAX_CHANNELS * 2 + 2)

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

typedef enum {
    RC_SEQ_OK,      /* correct next sequence  */
    RC_SEQ_GAP,     /* future sequence — packet loss detected */
    RC_SEQ_REPLAY,  /* past sequence — rejected */
} rc_seq_result_t;

/**
 * Encode packet into buffer. Returns encoded byte count, or -1 on error.
 * Buffer must be at least RC_PACKET_MAX_SIZE bytes.
 */
int rc_packet_encode(const rc_packet_t *packet, uint8_t *buf, size_t buf_size);

/**
 * Decode buffer into packet. Returns ESP_OK or ESP_ERR_INVALID_CRC / ESP_ERR_INVALID_ARG.
 */
esp_err_t rc_packet_decode(const uint8_t *buf, size_t len, rc_packet_t *packet);

/**
 * Validate monotonic sequence. Updates *last_seq on RC_SEQ_OK / RC_SEQ_GAP.
 */
rc_seq_result_t rc_protocol_check_sequence(uint16_t received, uint16_t *last_seq);
