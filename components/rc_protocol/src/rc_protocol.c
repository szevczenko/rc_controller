#include "rc_protocol.h"
#include <string.h>

/* Wire layout (little-endian):
 *   [0]     version
 *   [1]     type
 *   [2-3]   sequence  (LE)
 *   [4]     channel_count
 *   [5 .. 5+channel_count*2-1]  channels (LE int16)
 *   [last-1, last]  CRC-16/CCITT (LE), covers all bytes before it
 */

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF */
static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

int rc_packet_encode(const rc_packet_t *packet, uint8_t *buf, size_t buf_size)
{
    if (!packet || !buf) {
        return -1;
    }
    if (packet->channel_count > RC_MAX_CHANNELS) {
        return -1;
    }

    size_t payload_len = 5 + (size_t)packet->channel_count * 2;
    size_t total_len   = payload_len + 2; /* +2 for CRC */

    if (buf_size < total_len) {
        return -1;
    }

    buf[0] = packet->version;
    buf[1] = packet->type;
    buf[2] = (uint8_t)(packet->sequence & 0xFF);
    buf[3] = (uint8_t)(packet->sequence >> 8);
    buf[4] = packet->channel_count;

    for (uint8_t i = 0; i < packet->channel_count; i++) {
        uint16_t raw = (uint16_t)packet->channels[i];
        buf[5 + i * 2]     = (uint8_t)(raw & 0xFF);
        buf[5 + i * 2 + 1] = (uint8_t)(raw >> 8);
    }

    uint16_t crc = crc16(buf, payload_len);
    buf[payload_len]     = (uint8_t)(crc & 0xFF);
    buf[payload_len + 1] = (uint8_t)(crc >> 8);

    return (int)total_len;
}

esp_err_t rc_packet_decode(const uint8_t *buf, size_t len, rc_packet_t *packet)
{
    if (!buf || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    /* minimum: header(5) + 0 channels + crc(2) */
    if (len < 7) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t channel_count = buf[4];
    if (channel_count > RC_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t expected_len = 5 + (size_t)channel_count * 2 + 2;
    if (len != expected_len) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t payload_len = expected_len - 2;
    uint16_t crc_received = (uint16_t)buf[payload_len] | ((uint16_t)buf[payload_len + 1] << 8);
    uint16_t crc_computed = crc16(buf, payload_len);

    if (crc_received != crc_computed) {
        return ESP_ERR_INVALID_CRC;
    }

    packet->version       = buf[0];
    packet->type          = buf[1];
    packet->sequence      = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    packet->channel_count = channel_count;

    for (uint8_t i = 0; i < channel_count; i++) {
        uint16_t raw = (uint16_t)buf[5 + i * 2] | ((uint16_t)buf[5 + i * 2 + 1] << 8);
        packet->channels[i] = (int16_t)raw;
    }

    return ESP_OK;
}

rc_seq_result_t rc_protocol_check_sequence(uint16_t received, uint16_t *last_seq)
{
    uint16_t expected = *last_seq + 1;

    if (received == expected) {
        *last_seq = received;
        return RC_SEQ_OK;
    }
    /* uint16 wraps naturally; treat forward distance ≤ 32767 as a gap */
    uint16_t forward_dist = received - *last_seq;
    if (forward_dist > 0 && forward_dist <= 0x7FFF) {
        *last_seq = received;
        return RC_SEQ_GAP;
    }
    return RC_SEQ_REPLAY;
}

/* ── Telemetry encode/decode (MR-20) ────────────────────────────────────── */
/* Wire: version(1) type(1) rssi(1) lq(1) batt_mv(2LE) uptime(4LE) crc(2LE) */

int rc_telemetry_encode(const rc_telemetry_packet_t *pkt, uint8_t *buf, size_t buf_size)
{
    if (!pkt || !buf || buf_size < RC_TELEMETRY_PACKET_SIZE) {
        return -1;
    }
    buf[0] = pkt->version;
    buf[1] = pkt->type;
    buf[2] = (uint8_t)pkt->rssi;
    buf[3] = pkt->link_quality;
    buf[4] = (uint8_t)(pkt->battery_mv & 0xFF);
    buf[5] = (uint8_t)(pkt->battery_mv >> 8);
    buf[6] = (uint8_t)(pkt->uptime_ms & 0xFF);
    buf[7] = (uint8_t)((pkt->uptime_ms >> 8)  & 0xFF);
    buf[8] = (uint8_t)((pkt->uptime_ms >> 16) & 0xFF);
    buf[9] = (uint8_t)((pkt->uptime_ms >> 24) & 0xFF);

    uint16_t crc = crc16(buf, 10);
    buf[10] = (uint8_t)(crc & 0xFF);
    buf[11] = (uint8_t)(crc >> 8);
    return RC_TELEMETRY_PACKET_SIZE;
}

esp_err_t rc_telemetry_decode(const uint8_t *buf, size_t len, rc_telemetry_packet_t *pkt)
{
    if (!buf || !pkt || len != RC_TELEMETRY_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t crc_recv = (uint16_t)buf[10] | ((uint16_t)buf[11] << 8);
    if (crc_recv != crc16(buf, 10)) {
        return ESP_ERR_INVALID_CRC;
    }
    pkt->version      = buf[0];
    pkt->type         = buf[1];
    pkt->rssi         = (int8_t)buf[2];
    pkt->link_quality = buf[3];
    pkt->battery_mv   = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    pkt->uptime_ms    = (uint32_t)buf[6] | ((uint32_t)buf[7] << 8) |
                        ((uint32_t)buf[8] << 16) | ((uint32_t)buf[9] << 24);
    return ESP_OK;
}

