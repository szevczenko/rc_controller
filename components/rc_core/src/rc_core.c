#include "rc_core.h"
#include <string.h>

/* ── Failsafe state machine ─────────────────────────────────────────────── */

static struct {
    rc_link_state_t    state;
    uint32_t           last_packet_ms;
    uint32_t           timeout_ms;
    int16_t            failsafe_values[RC_MAX_CHANNELS];
    bool               initialized;
} s_ctx;

void rc_core_init(const rc_failsafe_config_t *config)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state      = RC_STATE_TIMEOUT;
    s_ctx.timeout_ms = (config && config->timeout_ms) ? config->timeout_ms
                                                       : RC_FAILSAFE_TIMEOUT_MS_DEFAULT;
    if (config) {
        memcpy(s_ctx.failsafe_values, config->failsafe_values, sizeof(s_ctx.failsafe_values));
    }
    s_ctx.initialized = true;
}

void rc_core_packet_received(uint32_t now_ms)
{
    s_ctx.last_packet_ms = now_ms;
    s_ctx.state          = RC_STATE_NORMAL;
}

void rc_core_tick(uint32_t now_ms)
{
    if (s_ctx.state == RC_STATE_NORMAL) {
        if ((now_ms - s_ctx.last_packet_ms) >= s_ctx.timeout_ms) {
            s_ctx.state = RC_STATE_FAILSAFE;
        }
    }
}

rc_link_state_t rc_core_get_link_state(void)
{
    return s_ctx.state;
}

const int16_t *rc_core_get_failsafe_values(void)
{
    return (s_ctx.state == RC_STATE_FAILSAFE) ? s_ctx.failsafe_values : NULL;
}

/* ── Normalization (MR-09) ──────────────────────────────────────────────── */

static int16_t clamp16(int32_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (int16_t)v;
}

int16_t rc_core_normalize(uint16_t raw, const rc_calibration_t *cal)
{
    if (!cal || cal->max == cal->min) {
        return 0;
    }
    if (raw <= cal->center) {
        uint16_t span = cal->center - cal->min;
        if (span == 0) return 0;
        int32_t v = -1000 + (int32_t)(raw - cal->min) * 1000 / span;
        return clamp16(v, -1000, 0);
    } else {
        uint16_t span = cal->max - cal->center;
        if (span == 0) return 0;
        int32_t v = (int32_t)(raw - cal->center) * 1000 / span;
        return clamp16(v, 0, 1000);
    }
}

/* ── Deadzone (MR-10) ───────────────────────────────────────────────────── */

int16_t rc_core_apply_deadzone(int16_t value, const rc_core_channel_config_t *cfg)
{
    if (!cfg || cfg->deadzone <= 0) {
        return value;
    }
    int16_t dz = cfg->deadzone;
    if (value >= -dz && value <= dz) {
        return 0;
    }
    /* Rescale outside deadzone so output still reaches ±1000. */
    int32_t range  = 1000 - dz;
    int32_t result;
    if (value > dz) {
        result = (int32_t)(value - dz) * 1000 / range;
    } else {
        result = (int32_t)(value + dz) * 1000 / range;
    }
    return clamp16(result, -1000, 1000);
}

/* ── Expo (MR-10.5) ─────────────────────────────────────────────────────── */

int16_t rc_core_apply_expo(int16_t value, uint8_t expo_factor)
{
    if (expo_factor == 0) {
        return value;
    }
    /* Blend linear and cubic: out = (1-k)*x + k*x^3,  k = expo_factor/100 */
    int32_t x    = value;                        /* -1000 … +1000 */
    int32_t x3   = x * x / 1000 * x / 1000;     /* x^3 scaled to same range */
    int32_t k    = expo_factor;                  /* 0..100 */
    int32_t out  = ((100 - k) * x + k * x3) / 100;
    return clamp16(out, -1000, 1000);
}

/* ── Arming (MR-23.5) ───────────────────────────────────────────────────── */

bool rc_arm_update(rc_arm_state_t *arm, const rc_channel_state_t *channels,
                   uint8_t arm_switch_ch, uint8_t throttle_ch)
{
    if (!arm || !channels) {
        return false;
    }
    if (arm_switch_ch >= channels->channel_count ||
        throttle_ch   >= channels->channel_count) {
        return false;
    }

    bool switch_on        = channels->channels[arm_switch_ch] > 0;
    bool throttle_neutral = (channels->channels[throttle_ch] >= -RC_ARM_THROTTLE_DEADZONE &&
                             channels->channels[throttle_ch] <=  RC_ARM_THROTTLE_DEADZONE);

    if (arm->armed) {
        if (!switch_on) {
            arm->armed = false;
        }
    } else {
        if (switch_on && throttle_neutral) {
            arm->armed = true;
        }
    }
    return arm->armed;
}

/* ── Link statistics (MR-19) ────────────────────────────────────────────── */

#define LQ_WINDOW 100

void rc_link_stats_update(rc_link_stats_t *stats, int rssi,
                          bool packet_lost, uint16_t sequence)
{
    if (!stats) return;
    stats->rssi          = (int8_t)rssi;
    stats->last_sequence = sequence;

    if (packet_lost) {
        stats->packets_lost++;
    } else {
        stats->packets_rx++;
    }

    uint32_t total = stats->packets_rx + stats->packets_lost;
    if (total == 0) {
        stats->link_quality = 0;
    } else {
        uint32_t window = total < LQ_WINDOW ? total : LQ_WINDOW;
        uint32_t lost_in_window = stats->packets_lost < window ? stats->packets_lost : window;
        stats->link_quality = (uint8_t)((window - lost_in_window) * 100 / window);
    }
}

void rc_link_stats_get(const rc_link_stats_t *stats, rc_link_stats_t *out)
{
    if (stats && out) {
        *out = *stats;
    }
}
