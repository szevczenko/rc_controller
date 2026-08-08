#pragma once

#include "rc_core.h"
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define RC_PROTOCOL_VERSION  1
/* wire size: header(5) + channels(RC_MAX_CHANNELS*2) + crc(2) */
#define RC_PACKET_MAX_SIZE   (5 + RC_MAX_CHANNELS * 2 + 2)

/* Telemetry: version(1) + type(1) + rssi(1) + lq(1) + batt_mv(2) + uptime(4) + crc(2) */
#define RC_TELEMETRY_PACKET_SIZE  12

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

/* MR-20: telemetry packet sent from RX → TX. */
typedef struct {
    uint8_t  version;
    uint8_t  type;      /* RC_PACKET_TYPE_TELEMETRY */
    int8_t   rssi;
    uint8_t  link_quality;
    uint16_t battery_mv;
    uint32_t uptime_ms;
} rc_telemetry_packet_t;

typedef enum {
    RC_SEQ_OK,      /* correct next sequence  */
    RC_SEQ_GAP,     /* future sequence — packet loss detected */
    RC_SEQ_REPLAY,  /* past sequence — rejected */
} rc_seq_result_t;

/* Returns encoded byte count, or -1 on error. */
int rc_packet_encode(const rc_packet_t *packet, uint8_t *buf, size_t buf_size);

/* Returns ESP_OK, ESP_ERR_INVALID_CRC, or ESP_ERR_INVALID_ARG. */
esp_err_t rc_packet_decode(const uint8_t *buf, size_t len, rc_packet_t *packet);

/* Encode telemetry packet. Returns encoded byte count, or -1 on error. */
int rc_telemetry_encode(const rc_telemetry_packet_t *pkt, uint8_t *buf, size_t buf_size);

/* Decode telemetry packet. Returns ESP_OK or ESP_ERR_INVALID_CRC. */
esp_err_t rc_telemetry_decode(const uint8_t *buf, size_t len, rc_telemetry_packet_t *pkt);

/* Validate monotonic sequence. Updates *last_seq on RC_SEQ_OK / RC_SEQ_GAP. */
rc_seq_result_t rc_protocol_check_sequence(uint16_t received, uint16_t *last_seq);

