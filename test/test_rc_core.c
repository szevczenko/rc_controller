#include "unity.h"
#include "rc_core.h"

void setUp(void) {}
void tearDown(void) {}

static rc_failsafe_config_t default_config(void)
{
    rc_failsafe_config_t cfg = {0};
    cfg.timeout_ms = 500;
    for (int i = 0; i < RC_MAX_CHANNELS; i++) {
        cfg.failsafe_values[i] = -1000;
    }
    return cfg;
}

void test_initial_state_is_timeout(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    TEST_ASSERT_EQUAL(RC_STATE_TIMEOUT, rc_core_get_link_state());
}

void test_packet_received_transitions_to_normal(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(1000);
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
}

void test_no_timeout_before_deadline(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(1000);
    rc_core_tick(1499); /* 499 ms elapsed — still normal */
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
}

void test_failsafe_triggers_at_timeout(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(1000);
    rc_core_tick(1500); /* exactly 500 ms elapsed */
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
}

void test_failsafe_triggers_after_timeout(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(600);
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
}

void test_failsafe_values_returned_in_failsafe_state(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(600);
    const int16_t *vals = rc_core_get_failsafe_values();
    TEST_ASSERT_NOT_NULL(vals);
    TEST_ASSERT_EQUAL_INT16(-1000, vals[0]);
}

void test_failsafe_values_null_in_normal_state(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(1000);
    TEST_ASSERT_NULL(rc_core_get_failsafe_values());
}

void test_recovery_from_failsafe(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(600); /* trigger failsafe */
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
    rc_core_packet_received(700); /* new packet arrives */
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
}

void test_custom_timeout(void)
{
    rc_failsafe_config_t cfg = {0};
    cfg.timeout_ms = 1000;
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(999);
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
    rc_core_tick(1000);
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
}

void test_default_timeout_when_zero(void)
{
    rc_failsafe_config_t cfg = {0}; /* timeout_ms = 0 → use default 500 ms */
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(499);
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
    rc_core_tick(500);
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_timeout);
    RUN_TEST(test_packet_received_transitions_to_normal);
    RUN_TEST(test_no_timeout_before_deadline);
    RUN_TEST(test_failsafe_triggers_at_timeout);
    RUN_TEST(test_failsafe_triggers_after_timeout);
    RUN_TEST(test_failsafe_values_returned_in_failsafe_state);
    RUN_TEST(test_failsafe_values_null_in_normal_state);
    RUN_TEST(test_recovery_from_failsafe);
    RUN_TEST(test_custom_timeout);
    RUN_TEST(test_default_timeout_when_zero);
    return UNITY_END();
}
