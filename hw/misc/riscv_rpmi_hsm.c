/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * RISC-V RPMI HSM service.
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"
#include "hw/core/cpu.h"
#include "librpmi_env.h"

static const struct rpmi_hsm_suspend_type riscv_rpmi_hsm_suspend_types[] = {
    {
        .type = 0,
        .info = {
            .flags = 0,
            .entry_latency_us = 0,
            .exit_latency_us = 0,
            .wakeup_latency_us = 0,
            .min_residency_us = 0,
        },
    },
};

static CPUState *riscv_rpmi_hart_cpu(RiscvRpmiState *s, uint32_t hart_index)
{
    if (hart_index >= s->hart_count || !s->hart_ids) {
        return NULL;
    }

    return cpu_by_arch_id(s->hart_ids[hart_index]);
}

static void riscv_rpmi_hsm_set_hw_state(RiscvRpmiState *s,
                                        uint32_t hart_index,
                                        enum rpmi_hart_hw_state state)
{
    CPUState *cpu = riscv_rpmi_hart_cpu(s, hart_index);

    if (hart_index >= s->hart_count || !s->hsm_hw_states) {
        return;
    }

    s->hsm_hw_states[hart_index] = state;
    if (!cpu) {
        return;
    }

    switch (state) {
    case RPMI_HART_HW_STATE_STARTED:
        cpu->halted = 0;
        cpu_resume(cpu);
        break;
    case RPMI_HART_HW_STATE_STOPPED:
    case RPMI_HART_HW_STATE_SUSPENDED:
        cpu->halted = 1;
        qemu_cpu_kick(cpu);
        break;
    default:
        break;
    }
}

static enum rpmi_error riscv_rpmi_hsm_start_prepare(
    void *priv, rpmi_uint32_t hart_index, rpmi_uint64_t start_addr)
{
    RiscvRpmiState *s = priv;
    CPUState *cpu = riscv_rpmi_hart_cpu(s, hart_index);

    if (!cpu) {
        return RPMI_ERR_INVALID_PARAM;
    }

    riscv_rpmi_hsm_set_hw_state(s, hart_index, RPMI_HART_HW_STATE_STARTED);
    return RPMI_SUCCESS;
}

static void riscv_rpmi_hsm_start_finalize(void *priv,
                                          rpmi_uint32_t hart_index,
                                          rpmi_uint64_t start_addr)
{
}

static enum rpmi_error riscv_rpmi_hsm_stop_prepare(void *priv,
                                                   rpmi_uint32_t hart_index)
{
    RiscvRpmiState *s = priv;

    if (!riscv_rpmi_hart_cpu(s, hart_index)) {
        return RPMI_ERR_INVALID_PARAM;
    }

    riscv_rpmi_hsm_set_hw_state(s, hart_index, RPMI_HART_HW_STATE_STOPPED);
    return RPMI_SUCCESS;
}

static void riscv_rpmi_hsm_stop_finalize(void *priv, rpmi_uint32_t hart_index)
{
}

static enum rpmi_error riscv_rpmi_hsm_suspend_prepare(
    void *priv, rpmi_uint32_t hart_index,
    const struct rpmi_hsm_suspend_type *suspend_type,
    rpmi_uint64_t resume_addr)
{
    RiscvRpmiState *s = priv;

    if (!suspend_type || !riscv_rpmi_hart_cpu(s, hart_index)) {
        return RPMI_ERR_INVALID_PARAM;
    }

    riscv_rpmi_hsm_set_hw_state(s, hart_index, RPMI_HART_HW_STATE_SUSPENDED);
    return RPMI_SUCCESS;
}

static void riscv_rpmi_hsm_suspend_finalize(
    void *priv, rpmi_uint32_t hart_index,
    const struct rpmi_hsm_suspend_type *suspend_type,
    rpmi_uint64_t resume_addr)
{
}

static enum rpmi_hart_hw_state riscv_rpmi_hsm_get_hw_state(
    void *priv, rpmi_uint32_t hart_index)
{
    RiscvRpmiState *s = priv;

    if (hart_index >= s->hart_count || !s->hsm_hw_states) {
        return RPMI_HART_HW_STATE_STOPPED;
    }

    return s->hsm_hw_states[hart_index];
}

static const struct rpmi_hsm_platform_ops riscv_rpmi_hsm_ops = {
    .hart_get_hw_state = riscv_rpmi_hsm_get_hw_state,
    .hart_start_prepare = riscv_rpmi_hsm_start_prepare,
    .hart_start_finalize = riscv_rpmi_hsm_start_finalize,
    .hart_stop_prepare = riscv_rpmi_hsm_stop_prepare,
    .hart_stop_finalize = riscv_rpmi_hsm_stop_finalize,
    .hart_suspend_prepare = riscv_rpmi_hsm_suspend_prepare,
    .hart_suspend_finalize = riscv_rpmi_hsm_suspend_finalize,
};

bool riscv_rpmi_hsm_create(RiscvRpmiState *s,
                                  struct rpmi_service_group **group,
                                  Error **errp)
{
    struct rpmi_hsm *hsm;

    if (!s->hart_count || !s->hart_ids) {
        error_setg(errp, "RPMI HSM service requires hart IDs");
        return false;
    }

    s->hsm_hw_states = g_new0(uint32_t, s->hart_count);
    for (uint32_t i = 0; i < s->hart_count; i++) {
        s->hsm_hw_states[i] = RPMI_HART_HW_STATE_STARTED;
    }

    hsm = rpmi_hsm_create(s->hart_count, s->hart_ids,
                          ARRAY_SIZE(riscv_rpmi_hsm_suspend_types),
                          riscv_rpmi_hsm_suspend_types,
                          &riscv_rpmi_hsm_ops, s);
    if (!hsm) {
        g_clear_pointer(&s->hsm_hw_states, g_free);
        error_setg(errp, "failed to create RPMI HSM context");
        return false;
    }

    *group = rpmi_service_group_hsm_create(hsm);
    if (!*group) {
        rpmi_hsm_destroy(hsm);
        g_clear_pointer(&s->hsm_hw_states, g_free);
        error_setg(errp, "failed to create RPMI HSM service group");
        return false;
    }

    s->hsm = hsm;
    return true;
}

void riscv_rpmi_hsm_destroy(RiscvRpmiState *s)
{
    if (s->hsm_group) {
        rpmi_env_free_lock(s->hsm_group->lock);
        s->hsm_group->lock = NULL;
        rpmi_service_group_hsm_destroy(s->hsm_group);
        s->hsm_group = NULL;
    }

    if (s->hsm) {
        rpmi_hsm_destroy(s->hsm);
        s->hsm = NULL;
    }

    g_clear_pointer(&s->hsm_hw_states, g_free);
}
