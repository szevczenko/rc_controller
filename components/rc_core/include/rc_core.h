#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RC_MAX_CHANNELS      16
#define RC_FAILSAFE_TIMEOUT_MS_DEFAULT  500

typedef struct {
    int16_t channels[RC_MAX_CHANNELS];
    uint8_t channel_count;
} rc_channel_state_t;

typedef enum {
    RC_STATE_NORMAL,    /* packets arriving on time */
    RC_STATE_TIMEOUT,   /* no packet for >= timeout_ms, waiting for failsafe */
    RC_STATE_FAILSAFE,  /* failsafe values applied */
} rc_link_state_t;

typedef struct {
    int16_t  failsafe_values[RC_MAX_CHANNELS];
    uint32_t timeout_ms; /* 0 = use default (500 ms) */
} rc_failsafe_config_t;

void rc_core_init(const rc_failsafe_config_t *config);

/* Call from radio receive callback — resets the timeout timer. */
void rc_core_packet_received(uint32_t now_ms);

/* Call periodically (e.g. every 10 ms) — advances timeout state machine. */
void rc_core_tick(uint32_t now_ms);

rc_link_state_t rc_core_get_link_state(void);

/* Returns failsafe channel values when in FAILSAFE state, else NULL. */
const int16_t *rc_core_get_failsafe_values(void);
