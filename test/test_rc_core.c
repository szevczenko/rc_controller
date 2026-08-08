#include "unity.h"
#include "rc_core.h"

void setUp(void) {}
void tearDown(void) {}

/* ── Failsafe ────────────────────────────────────────────────────────────── */

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
    rc_core_tick(1499);
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
}

void test_failsafe_triggers_at_timeout(void)
{
    rc_failsafe_config_t cfg = default_config();
    rc_core_init(&cfg);
    rc_core_packet_received(1000);
    rc_core_tick(1500);
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
    rc_core_tick(600);
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
    rc_core_packet_received(700);
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
    rc_failsafe_config_t cfg = {0};
    rc_core_init(&cfg);
    rc_core_packet_received(0);
    rc_core_tick(499);
    TEST_ASSERT_EQUAL(RC_STATE_NORMAL, rc_core_get_link_state());
    rc_core_tick(500);
    TEST_ASSERT_EQUAL(RC_STATE_FAILSAFE, rc_core_get_link_state());
}

/* ── Normalization (MR-09) ───────────────────────────────────────────────── */

void test_normalize_center(void)
{
    rc_calibration_t cal = {.min = 0, .center = 2048, .max = 4095};
    TEST_ASSERT_EQUAL_INT16(0, rc_core_normalize(2048, &cal));
}

void test_normalize_min(void)
{
    rc_calibration_t cal = {.min = 0, .center = 2048, .max = 4095};
    TEST_ASSERT_EQUAL_INT16(-1000, rc_core_normalize(0, &cal));
}

void test_normalize_max(void)
{
    rc_calibration_t cal = {.min = 0, .center = 2048, .max = 4095};
    TEST_ASSERT_EQUAL_INT16(1000, rc_core_normalize(4095, &cal));
}

void test_normalize_clamped_below_min(void)
{
    rc_calibration_t cal = {.min = 100, .center = 2048, .max = 4000};
    TEST_ASSERT_EQUAL_INT16(-1000, rc_core_normalize(0, &cal));
}

void test_normalize_null_cal(void)
{
    TEST_ASSERT_EQUAL_INT16(0, rc_core_normalize(2000, NULL));
}

/* ── Deadzone (MR-10) ────────────────────────────────────────────────────── */

void test_deadzone_center_zeroed(void)
{
    rc_core_channel_config_t cfg = {.deadzone = 100};
    TEST_ASSERT_EQUAL_INT16(0, rc_core_apply_deadzone(50, &cfg));
    TEST_ASSERT_EQUAL_INT16(0, rc_core_apply_deadzone(-50, &cfg));
    TEST_ASSERT_EQUAL_INT16(0, rc_core_apply_deadzone(0, &cfg));
}

void test_deadzone_max_reaches_1000(void)
{
    rc_core_channel_config_t cfg = {.deadzone = 100};
    TEST_ASSERT_EQUAL_INT16(1000, rc_core_apply_deadzone(1000, &cfg));
    TEST_ASSERT_EQUAL_INT16(-1000, rc_core_apply_deadzone(-1000, &cfg));
}

void test_deadzone_zero_passthrough(void)
{
    rc_core_channel_config_t cfg = {.deadzone = 0};
    TEST_ASSERT_EQUAL_INT16(500, rc_core_apply_deadzone(500, &cfg));
}

/* ── Expo (MR-10.5) ──────────────────────────────────────────────────────── */

void test_expo_zero_is_linear(void)
{
    TEST_ASSERT_EQUAL_INT16(500,  rc_core_apply_expo(500, 0));
    TEST_ASSERT_EQUAL_INT16(-500, rc_core_apply_expo(-500, 0));
    TEST_ASSERT_EQUAL_INT16(1000, rc_core_apply_expo(1000, 0));
}

void test_expo_reduces_small_inputs(void)
{
    /* At expo=100, small inputs should be reduced relative to linear. */
    int16_t linear = 200;
    int16_t expo   = rc_core_apply_expo(200, 100);
    TEST_ASSERT_LESS_THAN(linear, expo > 0 ? expo : -expo);
}

void test_expo_full_preserves_endpoints(void)
{
    TEST_ASSERT_EQUAL_INT16(1000,  rc_core_apply_expo(1000, 100));
    TEST_ASSERT_EQUAL_INT16(-1000, rc_core_apply_expo(-1000, 100));
    TEST_ASSERT_EQUAL_INT16(0,     rc_core_apply_expo(0, 100));
}

/* ── Arming (MR-23.5) ────────────────────────────────────────────────────── */

static rc_channel_state_t make_channels(int16_t thr, int16_t sw)
{
    rc_channel_state_t ch = {.channel_count = 3};
    ch.channels[0] = thr;
    ch.channels[2] = sw;
    return ch;
}

void test_arm_requires_switch_and_neutral_throttle(void)
{
    rc_arm_state_t arm = {0};
    rc_channel_state_t ch = make_channels(0, 1000);
    TEST_ASSERT_TRUE(rc_arm_update(&arm, &ch, 2, 0));
}

void test_arm_blocked_when_throttle_not_neutral(void)
{
    rc_arm_state_t arm = {0};
    rc_channel_state_t ch = make_channels(500, 1000);
    TEST_ASSERT_FALSE(rc_arm_update(&arm, &ch, 2, 0));
}

void test_arm_blocked_when_switch_off(void)
{
    rc_arm_state_t arm = {0};
    rc_channel_state_t ch = make_channels(0, -1000);
    TEST_ASSERT_FALSE(rc_arm_update(&arm, &ch, 2, 0));
}

void test_disarm_on_switch_off(void)
{
    rc_arm_state_t arm = {.armed = true};
    rc_channel_state_t ch = make_channels(500, -1000);
    TEST_ASSERT_FALSE(rc_arm_update(&arm, &ch, 2, 0));
}

void test_stays_armed_while_switch_on(void)
{
    rc_arm_state_t arm = {.armed = true};
    rc_channel_state_t ch = make_channels(800, 1000);
    TEST_ASSERT_TRUE(rc_arm_update(&arm, &ch, 2, 0));
}

/* ── Link stats (MR-19) ──────────────────────────────────────────────────── */

void test_link_stats_rssi_updated(void)
{
    rc_link_stats_t stats = {0};
    rc_link_stats_update(&stats, -55, false, 1);
    TEST_ASSERT_EQUAL_INT8(-55, stats.rssi);
}

void test_link_stats_counts_rx(void)
{
    rc_link_stats_t stats = {0};
    rc_link_stats_update(&stats, 0, false, 1);
    rc_link_stats_update(&stats, 0, false, 2);
    TEST_ASSERT_EQUAL_UINT32(2, stats.packets_rx);
    TEST_ASSERT_EQUAL_UINT32(0, stats.packets_lost);
}

void test_link_stats_counts_lost(void)
{
    rc_link_stats_t stats = {0};
    rc_link_stats_update(&stats, 0, false, 1);
    rc_link_stats_update(&stats, 0, true, 3);
    TEST_ASSERT_EQUAL_UINT32(1, stats.packets_lost);
}

void test_link_quality_100_when_no_loss(void)
{
    rc_link_stats_t stats = {0};
    for (int i = 0; i < 10; i++) {
        rc_link_stats_update(&stats, 0, false, (uint16_t)i);
    }
    TEST_ASSERT_EQUAL_UINT8(100, stats.link_quality);
}

int main(void)
{
    UNITY_BEGIN();
    /* Failsafe */
    RUN_TEST(test_initial_state_is_timeout);
    RUN_TEST(test_packet_received_transitions_to_normal);
    RUN_TEST(test_no_timeout_before_deadline);
    RUN_TEST(test_failsafe_triggers_at_timeout);
    RUN_TEST(test_failsafe_values_returned_in_failsafe_state);
    RUN_TEST(test_failsafe_values_null_in_normal_state);
    RUN_TEST(test_recovery_from_failsafe);
    RUN_TEST(test_custom_timeout);
    RUN_TEST(test_default_timeout_when_zero);
    /* Normalization */
    RUN_TEST(test_normalize_center);
    RUN_TEST(test_normalize_min);
    RUN_TEST(test_normalize_max);
    RUN_TEST(test_normalize_clamped_below_min);
    RUN_TEST(test_normalize_null_cal);
    /* Deadzone */
    RUN_TEST(test_deadzone_center_zeroed);
    RUN_TEST(test_deadzone_max_reaches_1000);
    RUN_TEST(test_deadzone_zero_passthrough);
    /* Expo */
    RUN_TEST(test_expo_zero_is_linear);
    RUN_TEST(test_expo_reduces_small_inputs);
    RUN_TEST(test_expo_full_preserves_endpoints);
    /* Arming */
    RUN_TEST(test_arm_requires_switch_and_neutral_throttle);
    RUN_TEST(test_arm_blocked_when_throttle_not_neutral);
    RUN_TEST(test_arm_blocked_when_switch_off);
    RUN_TEST(test_disarm_on_switch_off);
    RUN_TEST(test_stays_armed_while_switch_on);
    /* Link stats */
    RUN_TEST(test_link_stats_rssi_updated);
    RUN_TEST(test_link_stats_counts_rx);
    RUN_TEST(test_link_stats_counts_lost);
    RUN_TEST(test_link_quality_100_when_no_loss);
    return UNITY_END();
}
