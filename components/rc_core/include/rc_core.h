#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RC_MAX_CHANNELS                 16
#define RC_FAILSAFE_TIMEOUT_MS_DEFAULT  500
#define RC_CHANNEL_MIN                  (-1000)
#define RC_CHANNEL_MAX                  1000

typedef struct {
    int16_t channels[RC_MAX_CHANNELS];
    uint8_t channel_count;
} rc_channel_state_t;

/* ── Failsafe ────────────────────────────────────────────────────────────── */

typedef enum {
    RC_STATE_NORMAL,
    RC_STATE_TIMEOUT,
    RC_STATE_FAILSAFE,
} rc_link_state_t;

typedef struct {
    int16_t  failsafe_values[RC_MAX_CHANNELS];
    uint32_t timeout_ms;
} rc_failsafe_config_t;

void rc_core_init(const rc_failsafe_config_t *config);
void rc_core_packet_received(uint32_t now_ms);
void rc_core_tick(uint32_t now_ms);
rc_link_state_t  rc_core_get_link_state(void);
const int16_t   *rc_core_get_failsafe_values(void);

/* ── Calibration type (ADC → channel) ───────────────────────────────────── */

typedef struct {
    uint16_t min;
    uint16_t center;
    uint16_t max;
} rc_calibration_t;

/* MR-09: convert raw ADC (0–4095) to -1000…+1000 using calibration. */
int16_t rc_core_normalize(uint16_t raw, const rc_calibration_t *cal);

/* ── Deadzone / center (MR-10) ──────────────────────────────────────────── */

typedef struct {
    int16_t deadzone; /* symmetric deadzone half-width, e.g. 50 = ±5% */
} rc_core_channel_config_t;

/* Collapse values within ±deadzone to 0; rescale the rest to ±1000. */
int16_t rc_core_apply_deadzone(int16_t value, const rc_core_channel_config_t *cfg);

/* ── Expo (MR-10.5) ─────────────────────────────────────────────────────── */

/* expo_factor 0 = linear, 100 = max expo (y = x^3 like curve). */
int16_t rc_core_apply_expo(int16_t value, uint8_t expo_factor);

/* ── Arming (MR-23.5) ───────────────────────────────────────────────────── */

#define RC_ARM_THROTTLE_DEADZONE 50  /* counts within which throttle is "neutral" */

typedef struct {
    bool armed;
} rc_arm_state_t;

/*
 * Call on each tick. Returns true when arm condition is met (arm switch ON +
 * throttle at neutral). Caller must hold arm_state and pass it back each tick.
 */
bool rc_arm_update(rc_arm_state_t *arm, const rc_channel_state_t *channels,
                   uint8_t arm_switch_ch, uint8_t throttle_ch);

/* ── Link statistics (MR-19) ────────────────────────────────────────────── */

typedef struct {
    int8_t   rssi;
    uint32_t packets_rx;
    uint32_t packets_lost;
    uint8_t  link_quality; /* 0–100 % rolling window */
    uint16_t last_sequence;
} rc_link_stats_t;

/* packet_lost = true when a gap or replay was detected. */
void rc_link_stats_update(rc_link_stats_t *stats, int rssi,
                          bool packet_lost, uint16_t sequence);
void rc_link_stats_get(const rc_link_stats_t *stats, rc_link_stats_t *out);

