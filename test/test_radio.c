#include "unity.h"
#include "radio.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static uint8_t  s_rx_buf[256];
static size_t   s_rx_len;
static int      s_rx_rssi;
static int      s_rx_count;

static void test_rx_cb(const uint8_t *data, size_t len, int rssi)
{
    s_rx_len  = len < sizeof(s_rx_buf) ? len : sizeof(s_rx_buf);
    memcpy(s_rx_buf, data, s_rx_len);
    s_rx_rssi = rssi;
    s_rx_count++;
}

static void reset_rx(void)
{
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    s_rx_len   = 0;
    s_rx_rssi  = 0;
    s_rx_count = 0;
}

void test_loopback_send_fires_rx_cb(void)
{
    reset_rx();
    TEST_ASSERT_EQUAL(ESP_OK, radio_init(radio_loopback_get_driver(), test_rx_cb));

    const uint8_t payload[] = {0x01, 0x02, 0x03};
    TEST_ASSERT_EQUAL(ESP_OK, radio_send(payload, sizeof(payload)));

    TEST_ASSERT_EQUAL_INT(1, s_rx_count);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), s_rx_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, s_rx_buf, sizeof(payload));
}

void test_loopback_rssi_is_zero(void)
{
    reset_rx();
    radio_init(radio_loopback_get_driver(), test_rx_cb);
    const uint8_t b = 0xFF;
    radio_send(&b, 1);
    TEST_ASSERT_EQUAL_INT(0, s_rx_rssi);
    TEST_ASSERT_EQUAL_INT(0, radio_get_rssi());
}

void test_loopback_set_channel_ok(void)
{
    radio_init(radio_loopback_get_driver(), test_rx_cb);
    TEST_ASSERT_EQUAL(ESP_OK, radio_set_channel(6));
}

void test_send_before_init_fails(void)
{
    /* radio_send should return ESP_ERR_INVALID_STATE when no driver is set. */
    const uint8_t b = 0xAA;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, radio_send(&b, 1));
    /* Also verify NULL driver is rejected. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, radio_init(NULL, test_rx_cb));
}

void test_loopback_multiple_sends(void)
{
    reset_rx();
    radio_init(radio_loopback_get_driver(), test_rx_cb);

    const uint8_t p1[] = {0xAA};
    const uint8_t p2[] = {0xBB, 0xCC};
    radio_send(p1, sizeof(p1));
    radio_send(p2, sizeof(p2));

    /* Last received packet should be p2 */
    TEST_ASSERT_EQUAL_INT(2, s_rx_count);
    TEST_ASSERT_EQUAL_size_t(sizeof(p2), s_rx_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p2, s_rx_buf, sizeof(p2));
}

void test_loopback_driver_name(void)
{
    TEST_ASSERT_EQUAL_STRING("loopback", radio_loopback_get_driver()->name);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_send_before_init_fails);
    RUN_TEST(test_loopback_send_fires_rx_cb);
    RUN_TEST(test_loopback_rssi_is_zero);
    RUN_TEST(test_loopback_set_channel_ok);
    RUN_TEST(test_loopback_multiple_sends);
    RUN_TEST(test_loopback_driver_name);
    return UNITY_END();
}
