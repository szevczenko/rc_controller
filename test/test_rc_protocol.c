#include "unity.h"
#include <string.h>
#include "rc_protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_encode_decode_roundtrip(void)
{
    rc_packet_t tx = {
        .version       = RC_PROTOCOL_VERSION,
        .type          = RC_PACKET_TYPE_RC,
        .sequence      = 42,
        .channel_count = 4,
        .channels      = {0, 1000, -1000, 500},
    };

    uint8_t buf[RC_PACKET_MAX_SIZE];
    int len = rc_packet_encode(&tx, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);

    rc_packet_t rx = {0};
    TEST_ASSERT_EQUAL(ESP_OK, rc_packet_decode(buf, (size_t)len, &rx));
    TEST_ASSERT_EQUAL_UINT8(tx.version,       rx.version);
    TEST_ASSERT_EQUAL_UINT8(tx.type,          rx.type);
    TEST_ASSERT_EQUAL_UINT16(tx.sequence,     rx.sequence);
    TEST_ASSERT_EQUAL_UINT8(tx.channel_count, rx.channel_count);
    TEST_ASSERT_EQUAL_INT16_ARRAY(tx.channels, rx.channels, tx.channel_count);
}

void test_crc_corruption_detected(void)
{
    rc_packet_t tx = {
        .version       = RC_PROTOCOL_VERSION,
        .type          = RC_PACKET_TYPE_RC,
        .sequence      = 1,
        .channel_count = 2,
        .channels      = {100, -100},
    };

    uint8_t buf[RC_PACKET_MAX_SIZE];
    int len = rc_packet_encode(&tx, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);

    buf[3] ^= 0xFF; /* corrupt one byte */

    rc_packet_t rx = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, rc_packet_decode(buf, (size_t)len, &rx));
}

void test_sequence_ok(void)
{
    uint16_t last = 10;
    TEST_ASSERT_EQUAL(RC_SEQ_OK, rc_protocol_check_sequence(11, &last));
    TEST_ASSERT_EQUAL_UINT16(11, last);
}

void test_sequence_gap(void)
{
    uint16_t last = 10;
    TEST_ASSERT_EQUAL(RC_SEQ_GAP, rc_protocol_check_sequence(15, &last));
    TEST_ASSERT_EQUAL_UINT16(15, last);
}

void test_sequence_replay(void)
{
    uint16_t last = 10;
    TEST_ASSERT_EQUAL(RC_SEQ_REPLAY, rc_protocol_check_sequence(9, &last));
    TEST_ASSERT_EQUAL_UINT16(10, last); /* unchanged */
}

void test_sequence_wrap(void)
{
    uint16_t last = 0xFFFF;
    TEST_ASSERT_EQUAL(RC_SEQ_OK, rc_protocol_check_sequence(0, &last));
    TEST_ASSERT_EQUAL_UINT16(0, last);
}

void test_max_channels(void)
{
    rc_packet_t tx = {
        .version       = RC_PROTOCOL_VERSION,
        .type          = RC_PACKET_TYPE_RC,
        .sequence      = 0,
        .channel_count = RC_MAX_CHANNELS,
    };
    for (int i = 0; i < RC_MAX_CHANNELS; i++) {
        tx.channels[i] = (int16_t)(i * 100 - 500);
    }

    uint8_t buf[RC_PACKET_MAX_SIZE];
    int len = rc_packet_encode(&tx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(RC_PACKET_MAX_SIZE, len);

    rc_packet_t rx = {0};
    TEST_ASSERT_EQUAL(ESP_OK, rc_packet_decode(buf, (size_t)len, &rx));
    TEST_ASSERT_EQUAL_INT16_ARRAY(tx.channels, rx.channels, RC_MAX_CHANNELS);
}

void test_encode_null_args(void)
{
    uint8_t buf[RC_PACKET_MAX_SIZE];
    TEST_ASSERT_EQUAL_INT(-1, rc_packet_encode(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, rc_packet_encode((rc_packet_t *)buf, NULL, sizeof(buf)));
}

void test_decode_null_args(void)
{
    uint8_t buf[RC_PACKET_MAX_SIZE] = {0};
    rc_packet_t pkt;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, rc_packet_decode(NULL, sizeof(buf), &pkt));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, rc_packet_decode(buf, sizeof(buf), NULL));
}

/* ── Telemetry (MR-20) ───────────────────────────────────────────────────── */

void test_telemetry_roundtrip(void)
{
    rc_telemetry_packet_t tx = {
        .version      = RC_PROTOCOL_VERSION,
        .type         = RC_PACKET_TYPE_TELEMETRY,
        .rssi         = -67,
        .link_quality = 95,
        .battery_mv   = 7400,
        .uptime_ms    = 12345678,
    };
    uint8_t buf[RC_TELEMETRY_PACKET_SIZE];
    int len = rc_telemetry_encode(&tx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(RC_TELEMETRY_PACKET_SIZE, len);

    rc_telemetry_packet_t rx = {0};
    TEST_ASSERT_EQUAL(ESP_OK, rc_telemetry_decode(buf, (size_t)len, &rx));
    TEST_ASSERT_EQUAL_INT8(tx.rssi,         rx.rssi);
    TEST_ASSERT_EQUAL_UINT8(tx.link_quality, rx.link_quality);
    TEST_ASSERT_EQUAL_UINT16(tx.battery_mv,  rx.battery_mv);
    TEST_ASSERT_EQUAL_UINT32(tx.uptime_ms,   rx.uptime_ms);
}

void test_telemetry_crc_detected(void)
{
    rc_telemetry_packet_t tx = {
        .version = RC_PROTOCOL_VERSION,
        .type    = RC_PACKET_TYPE_TELEMETRY,
        .rssi    = -50,
    };
    uint8_t buf[RC_TELEMETRY_PACKET_SIZE];
    rc_telemetry_encode(&tx, buf, sizeof(buf));
    buf[2] ^= 0xFF;
    rc_telemetry_packet_t rx;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, rc_telemetry_decode(buf, sizeof(buf), &rx));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_decode_roundtrip);
    RUN_TEST(test_crc_corruption_detected);
    RUN_TEST(test_sequence_ok);
    RUN_TEST(test_sequence_gap);
    RUN_TEST(test_sequence_replay);
    RUN_TEST(test_sequence_wrap);
    RUN_TEST(test_max_channels);
    RUN_TEST(test_encode_null_args);
    RUN_TEST(test_decode_null_args);
    RUN_TEST(test_telemetry_roundtrip);
    RUN_TEST(test_telemetry_crc_detected);
    return UNITY_END();
}
