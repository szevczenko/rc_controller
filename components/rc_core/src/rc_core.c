#include "rc_core.h"
#include <string.h>

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
    s_ctx.state      = RC_STATE_TIMEOUT; /* no packet yet */
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
        uint32_t elapsed = now_ms - s_ctx.last_packet_ms;
        if (elapsed >= s_ctx.timeout_ms) {
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
    if (s_ctx.state == RC_STATE_FAILSAFE) {
        return s_ctx.failsafe_values;
    }
    return NULL;
}
