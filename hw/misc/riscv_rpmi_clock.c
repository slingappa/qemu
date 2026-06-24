/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI Clock service.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"

static const uint64_t riscv_rpmi_clock_rate_linear[] = {
    0x1111111122222222ULL, 0xbbbbbbbbccccccccULL, 0x2222222222222222ULL,
};

static const uint64_t riscv_rpmi_clock_rate_discrete[] = {
    0x1111111122222222ULL, 0x2222222233333333ULL,
    0x3333333344444444ULL, 0x4444444455555555ULL,
    0x5555555566666666ULL, 0x6666666677777777ULL,
};

static const struct rpmi_clock_data riscv_rpmi_clock_data[] = {
    {
        .name = "clock0",
        .parent_id = -1U,
        .transition_latency_ms = 100,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_linear),
        .clock_type = RPMI_CLK_TYPE_LINEAR,
        .clock_rate_array = riscv_rpmi_clock_rate_linear,
    }, {
        .name = "clock1",
        .parent_id = 0,
        .transition_latency_ms = 100,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_linear),
        .clock_type = RPMI_CLK_TYPE_LINEAR,
        .clock_rate_array = riscv_rpmi_clock_rate_linear,
    }, {
        .name = "clock2",
        .parent_id = 0,
        .transition_latency_ms = 50,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_discrete),
        .clock_type = RPMI_CLK_TYPE_DISCRETE,
        .clock_rate_array = riscv_rpmi_clock_rate_discrete,
    }, {
        .name = "clock3",
        .parent_id = 1,
        .transition_latency_ms = 50,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_discrete),
        .clock_type = RPMI_CLK_TYPE_DISCRETE,
        .clock_rate_array = riscv_rpmi_clock_rate_discrete,
    }, {
        .name = "clock4",
        .parent_id = 1,
        .transition_latency_ms = 100,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_linear),
        .clock_type = RPMI_CLK_TYPE_LINEAR,
        .clock_rate_array = riscv_rpmi_clock_rate_linear,
    }, {
        .name = "clock5",
        .parent_id = 4,
        .transition_latency_ms = 50,
        .rate_count = ARRAY_SIZE(riscv_rpmi_clock_rate_discrete),
        .clock_type = RPMI_CLK_TYPE_DISCRETE,
        .clock_rate_array = riscv_rpmi_clock_rate_discrete,
    },
};

static const RiscvRpmiClockState riscv_rpmi_clock_reset_state[] = {
    [0] = { RPMI_CLK_STATE_ENABLED, 0x1111111122222222ULL },
    [1] = { RPMI_CLK_STATE_ENABLED, 0xbbbbbbbbccccccccULL },
    [2] = { RPMI_CLK_STATE_ENABLED, 0x2222222233333333ULL },
    [3] = { RPMI_CLK_STATE_ENABLED, 0x3333333344444444ULL },
    [4] = { RPMI_CLK_STATE_ENABLED, 0x1111111122222222ULL },
    [5] = { RPMI_CLK_STATE_ENABLED, 0x5555555566666666ULL },
};

typedef struct RiscvRpmiClockRateMatch {
    enum rpmi_clock_rate_match match;
    int64_t adjustment;
} RiscvRpmiClockRateMatch;

static const RiscvRpmiClockRateMatch riscv_rpmi_clock_rate_matches[] = {
    {
        .match = RPMI_CLK_RATE_MATCH_ROUND_UP,
        .adjustment = 0x100,
    }, {
        .match = RPMI_CLK_RATE_MATCH_ROUND_DOWN,
        .adjustment = -0x100,
    }, {
        .match = RPMI_CLK_RATE_MATCH_PLATFORM,
        .adjustment = 0x200,
    },
};

static const RiscvRpmiClockRateMatch *riscv_rpmi_clock_rate_match_by_id(
    enum rpmi_clock_rate_match match)
{
    for (uint32_t index = 0;
         index < ARRAY_SIZE(riscv_rpmi_clock_rate_matches); index++) {
        if (riscv_rpmi_clock_rate_matches[index].match == match) {
            return &riscv_rpmi_clock_rate_matches[index];
        }
    }

    return NULL;
}

static enum rpmi_error riscv_rpmi_clock_set_state(
    void *priv, uint32_t clock_id, enum rpmi_clock_state state)
{
    RiscvRpmiState *s = priv;

    if (clock_id >= RISCV_RPMI_CLOCK_COUNT || !s->clock_state ||
        state >= RPMI_CLK_STATE_MAX) {
        return RPMI_ERR_INVALID_PARAM;
    }

    s->clock_state[clock_id].state = state;
    return RPMI_SUCCESS;
}

static enum rpmi_error riscv_rpmi_clock_get_state_rate(
    void *priv, uint32_t clock_id, enum rpmi_clock_state *state,
    uint64_t *rate)
{
    RiscvRpmiState *s = priv;

    if (clock_id >= RISCV_RPMI_CLOCK_COUNT || !s->clock_state ||
        (!state && !rate)) {
        return RPMI_ERR_INVALID_PARAM;
    }

    if (state) {
        *state = s->clock_state[clock_id].state;
    }
    if (rate) {
        *rate = s->clock_state[clock_id].rate;
    }

    return RPMI_SUCCESS;
}

static rpmi_bool_t riscv_rpmi_clock_rate_change_match(void *priv,
                                                      uint32_t clock_id,
                                                      uint64_t rate)
{
    RiscvRpmiState *s = priv;
    uint64_t current_rate;

    if (clock_id >= RISCV_RPMI_CLOCK_COUNT || !s->clock_state) {
        return false;
    }

    current_rate = s->clock_state[clock_id].rate;
    if (rate > current_rate && rate - current_rate > 0x100) {
        return true;
    }
    if (rate < current_rate && current_rate - rate < 0x100) {
        return true;
    }

    return false;
}

static enum rpmi_error riscv_rpmi_clock_set_rate(
    void *priv, uint32_t clock_id, enum rpmi_clock_rate_match match,
    uint64_t rate, uint64_t *new_rate)
{
    RiscvRpmiState *s = priv;
    const RiscvRpmiClockRateMatch *rate_match;

    if (clock_id >= RISCV_RPMI_CLOCK_COUNT || !s->clock_state ||
        !new_rate) {
        return RPMI_ERR_INVALID_PARAM;
    }

    if (!riscv_rpmi_clock_rate_change_match(priv, clock_id, rate)) {
        return RPMI_ERR_ALREADY;
    }

    rate_match = riscv_rpmi_clock_rate_match_by_id(match);
    if (!rate_match) {
        return RPMI_ERR_INVALID_PARAM;
    }

    if (rate_match->adjustment < 0) {
        s->clock_state[clock_id].rate =
            rate - (uint64_t)-rate_match->adjustment;
    } else {
        s->clock_state[clock_id].rate = rate + rate_match->adjustment;
    }

    *new_rate = s->clock_state[clock_id].rate;
    return RPMI_SUCCESS;
}

static enum rpmi_error riscv_rpmi_clock_set_rate_recalc(
    void *priv, uint32_t clock_id, uint64_t parent_rate, uint64_t *new_rate)
{
    RiscvRpmiState *s = priv;

    if (clock_id >= RISCV_RPMI_CLOCK_COUNT || !s->clock_state ||
        !parent_rate || !new_rate) {
        return RPMI_ERR_INVALID_PARAM;
    }

    s->clock_state[clock_id].rate =
        (s->clock_state[clock_id].rate / parent_rate) * 3 / 2;
    *new_rate = s->clock_state[clock_id].rate;
    return RPMI_SUCCESS;
}

static const struct rpmi_clock_platform_ops riscv_rpmi_clock_ops = {
    .set_state = riscv_rpmi_clock_set_state,
    .get_state_and_rate = riscv_rpmi_clock_get_state_rate,
    .rate_change_match = riscv_rpmi_clock_rate_change_match,
    .set_rate = riscv_rpmi_clock_set_rate,
    .set_rate_recalc = riscv_rpmi_clock_set_rate_recalc,
};

static bool riscv_rpmi_clock_create(RiscvRpmiState *s,
                                    struct rpmi_service_group **group,
                                    Error **errp)
{
    s->clock_state = g_memdup2(riscv_rpmi_clock_reset_state,
                               sizeof(riscv_rpmi_clock_reset_state));
    *group = rpmi_service_group_clock_create(
        ARRAY_SIZE(riscv_rpmi_clock_data), riscv_rpmi_clock_data,
        &riscv_rpmi_clock_ops, s);
    if (!*group) {
        g_clear_pointer(&s->clock_state, g_free);
        error_setg(errp, "failed to create RPMI clock service group");
        return false;
    }

    return true;
}

static void riscv_rpmi_clock_destroy(RiscvRpmiState *s)
{
    if (s->clock_group) {
        rpmi_service_group_clock_destroy(s->clock_group);
        s->clock_group = NULL;
    }

    g_clear_pointer(&s->clock_state, g_free);
}

bool riscv_rpmi_clock_add(RiscvRpmiState *s, Error **errp)
{
    struct rpmi_service_group *group;

    if (s->clock_group) {
        error_setg(errp, "duplicate RPMI clock service descriptor");
        return false;
    }

    if (!riscv_rpmi_clock_create(s, &group, errp)) {
        return false;
    }

    if (!riscv_rpmi_context_add_group(s, group, "clock", errp)) {
        s->clock_group = group;
        riscv_rpmi_clock_destroy(s);
        return false;
    }

    s->clock_group = group;
    return true;
}

void riscv_rpmi_clock_remove(RiscvRpmiState *s)
{
    riscv_rpmi_context_remove_group(s, s->clock_group);
    riscv_rpmi_clock_destroy(s);
}
