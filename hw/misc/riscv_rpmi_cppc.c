/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI CPPC service.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@qti.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"
#include "qemu/timer.h"
#include "system/address-spaces.h"

void riscv_rpmi_cppc_configure(RiscvRpmiState *s,
                                const RiscvRpmiConfig *cfg)
{
    s->cppc_fastchan_base = cfg->cppc_fastchan_base;
    s->cppc_fastchan_size = cfg->cppc_fastchan_size;
    s->cppc_perf_request_offset = cfg->cppc_perf_request_offset;
    s->cppc_perf_feedback_offset = cfg->cppc_perf_feedback_offset;
    if (cfg->cppc_profile) {
        s->cppc_profile = *cfg->cppc_profile;
        s->has_cppc_profile = true;
    }
}

static uint64_t riscv_rpmi_cppc_elapsed_ns(RiscvRpmiState *s)
{
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    if (now <= s->cppc_counter_base_ns) {
        return 0;
    }

    return now - s->cppc_counter_base_ns;
}

static enum rpmi_error riscv_rpmi_cppc_get_reg(
    void *priv, rpmi_uint32_t reg_id, rpmi_uint32_t hart_index,
    rpmi_uint64_t *val)
{
    RiscvRpmiState *s = priv;
    uint64_t elapsed_ns;
    uint64_t desired_perf;

    if (!val || hart_index >= s->hart_count) {
        return RPMI_ERR_INVALID_PARAM;
    }

    elapsed_ns = riscv_rpmi_cppc_elapsed_ns(s);
    desired_perf = s->cppc_desired_perf[hart_index];

    switch (reg_id) {
    case RPMI_CPPC_REFERENCE_PERF_COUNTER:
        *val = muldiv64(elapsed_ns, s->cppc_profile.reference_perf,
                        NANOSECONDS_PER_SECOND);
        return RPMI_SUCCESS;
    case RPMI_CPPC_DELIVERED_PERF_COUNTER:
        *val = muldiv64(elapsed_ns, desired_perf, NANOSECONDS_PER_SECOND);
        return RPMI_SUCCESS;
    case RPMI_CPPC_PERF_LIMITED:
        *val = 0;
        return RPMI_SUCCESS;
    default:
        return RPMI_ERR_NOTSUPP;
    }
}

static enum rpmi_error riscv_rpmi_cppc_set_reg(
    void *priv, rpmi_uint32_t reg_id, rpmi_uint32_t hart_index,
    rpmi_uint64_t val)
{
    RiscvRpmiState *s = priv;

    if (hart_index >= s->hart_count) {
        return RPMI_ERR_INVALID_PARAM;
    }

    switch (reg_id) {
    case RPMI_CPPC_DESIRED_PERF:
        if (val < s->cppc_profile.lowest_perf ||
            val > s->cppc_profile.highest_perf) {
            return RPMI_ERR_INVALID_PARAM;
        }
        s->cppc_desired_perf[hart_index] = val;
        return RPMI_SUCCESS;
    default:
        return RPMI_ERR_DENIED;
    }
}

static enum rpmi_error riscv_rpmi_cppc_update_perf(
    void *priv, rpmi_uint32_t hart_index, rpmi_uint32_t desired_perf)
{
    RiscvRpmiState *s = priv;

    if (hart_index >= s->hart_count) {
        return RPMI_ERR_INVALID_PARAM;
    }

    if (desired_perf < s->cppc_profile.lowest_perf ||
        desired_perf > s->cppc_profile.highest_perf) {
        return RPMI_ERR_INVALID_PARAM;
    }

    s->cppc_desired_perf[hart_index] = desired_perf;
    return RPMI_SUCCESS;
}

static enum rpmi_error riscv_rpmi_cppc_get_current_freq(
    void *priv, rpmi_uint32_t hart_index, rpmi_uint64_t *current_freq_hz)
{
    RiscvRpmiState *s = priv;
    uint64_t desired_perf;
    uint64_t nominal_perf;
    uint64_t nominal_freq_hz;

    if (!current_freq_hz || hart_index >= s->hart_count) {
        return RPMI_ERR_INVALID_PARAM;
    }

    nominal_perf = s->cppc_profile.nominal_perf;
    if (!nominal_perf) {
        return RPMI_ERR_INVALID_PARAM;
    }

    desired_perf = s->cppc_desired_perf[hart_index];
    nominal_freq_hz = (uint64_t)s->cppc_profile.nominal_freq *
                      RPMI_CPPC_FREQ_MHZ_TO_HZ;
    *current_freq_hz = muldiv64(nominal_freq_hz, desired_perf, nominal_perf);
    return RPMI_SUCCESS;
}

static const struct rpmi_cppc_platform_ops riscv_rpmi_cppc_ops = {
    .cppc_get_reg = riscv_rpmi_cppc_get_reg,
    .cppc_set_reg = riscv_rpmi_cppc_set_reg,
    .cppc_update_perf = riscv_rpmi_cppc_update_perf,
    .cppc_get_current_freq = riscv_rpmi_cppc_get_current_freq,
};
static void riscv_rpmi_cppc_reset_state(RiscvRpmiState *s)
{
    uint32_t i;

    if (!s->cppc_desired_perf) {
        return;
    }

    for (i = 0; i < s->hart_count; i++) {
        uint32_t desired_perf = s->cppc_profile.nominal_perf;

        s->cppc_desired_perf[i] = desired_perf;
        if (s->cppc_fastchan_shmem) {
            rpmi_shmem_write(s->cppc_fastchan_shmem,
                             s->cppc_perf_request_offset +
                             i * sizeof(union rpmi_cppc_perf_request_fastchan),
                             &desired_perf, sizeof(desired_perf));
        }
    }

    s->cppc_counter_base_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
}

static bool riscv_rpmi_cppc_create(RiscvRpmiState *s,
                                   struct rpmi_service_group **group,
                                   Error **errp)
{

    if (!s->hsm) {
        error_setg(errp, "RPMI CPPC service requires HSM service");
        return false;
    }

    if (!s->has_cppc_profile) {
        error_setg(errp, "RPMI CPPC service requires a CPPC profile");
        return false;
    }

    s->cppc_regs = g_new0(struct rpmi_cppc_regs, 1);
    s->cppc_regs->highest_perf = s->cppc_profile.highest_perf;
    s->cppc_regs->nominal_perf = s->cppc_profile.nominal_perf;
    s->cppc_regs->lowest_nonlinear_perf =
        s->cppc_profile.lowest_nonlinear_perf;
    s->cppc_regs->lowest_perf = s->cppc_profile.lowest_perf;
    s->cppc_regs->reference_perf = s->cppc_profile.reference_perf;
    s->cppc_regs->lowest_freq = s->cppc_profile.lowest_freq;
    s->cppc_regs->nominal_freq = s->cppc_profile.nominal_freq;
    s->cppc_regs->transition_latency = s->cppc_profile.transition_latency;

    s->cppc_desired_perf = g_new0(uint32_t, s->hart_count);

    s->cppc_fastchan_shmem =
        rpmi_shmem_create("rpmi-cppc-fastchan", s->cppc_fastchan_base,
                          s->cppc_fastchan_size, &rpmi_shmem_qemu_ops, s);
    if (!s->cppc_fastchan_shmem) {
        error_setg(errp, "failed to create RPMI CPPC fast channel shmem");
        g_clear_pointer(&s->cppc_desired_perf, g_free);
        g_clear_pointer(&s->cppc_regs, g_free);
        return false;
    }

    *group = rpmi_service_group_cppc_create(s->hsm, s->cppc_regs,
                                            RPMI_CPPC_PASSIVE_MODE,
                                            s->cppc_fastchan_shmem,
                                            s->cppc_perf_request_offset,
                                            s->cppc_perf_feedback_offset,
                                            &riscv_rpmi_cppc_ops, s);
    if (!*group) {
        rpmi_shmem_destroy(s->cppc_fastchan_shmem);
        s->cppc_fastchan_shmem = NULL;
        g_clear_pointer(&s->cppc_desired_perf, g_free);
        g_clear_pointer(&s->cppc_regs, g_free);
        error_setg(errp, "failed to create RPMI CPPC service group");
        return false;
    }

    riscv_rpmi_cppc_reset_state(s);
    return true;
}

static void riscv_rpmi_cppc_destroy(RiscvRpmiState *s)
{
    if (s->cppc_group) {
        rpmi_service_group_cppc_destroy(s->cppc_group);
        s->cppc_group = NULL;
    }

    if (s->cppc_fastchan_shmem) {
        rpmi_shmem_destroy(s->cppc_fastchan_shmem);
        s->cppc_fastchan_shmem = NULL;
    }

    g_clear_pointer(&s->cppc_desired_perf, g_free);
    g_clear_pointer(&s->cppc_regs, g_free);
}

bool riscv_rpmi_cppc_add(RiscvRpmiState *s, Error **errp)
{
    struct rpmi_service_group *group;

    if (s->cppc_group) {
        error_setg(errp, "duplicate RPMI CPPC service descriptor");
        return false;
    }

    if (!riscv_rpmi_cppc_create(s, &group, errp)) {
        return false;
    }

    s->cppc_group = group;
    if (!riscv_rpmi_context_add_group(s, group, "CPPC", errp)) {
        riscv_rpmi_cppc_destroy(s);
        return false;
    }

    return true;
}

void riscv_rpmi_cppc_remove(RiscvRpmiState *s)
{
    riscv_rpmi_context_remove_group(s, s->cppc_group);
    riscv_rpmi_cppc_destroy(s);
}

bool riscv_rpmi_cppc_realize_fastchan(RiscvRpmiState *s, DeviceState *dev,
                                      Error **errp)
{
    g_autofree char *fastchan_name = NULL;

    if (!riscv_rpmi_service_enabled(s, RISCV_RPMI_SERVICE_CPPC)) {
        return true;
    }

    fastchan_name = g_strdup_printf("rpmi-cppc-fastchan@%" PRIx64,
                                    s->cppc_fastchan_base);
    if (!memory_region_init_ram(&s->cppc_fastchan, OBJECT(dev),
                                fastchan_name, s->cppc_fastchan_size,
                                errp)) {
        return false;
    }

    memory_region_add_subregion(get_system_memory(), s->cppc_fastchan_base,
                                &s->cppc_fastchan);
    s->has_cppc_fastchan = true;
    return true;
}

void riscv_rpmi_cppc_unrealize_fastchan(RiscvRpmiState *s)
{
    if (s->has_cppc_fastchan) {
        memory_region_del_subregion(get_system_memory(), &s->cppc_fastchan);
        s->has_cppc_fastchan = false;
    }
}

void riscv_rpmi_cppc_reset_fastchan(RiscvRpmiState *s)
{
    if (!s->has_cppc_fastchan) {
        return;
    }

    memset(memory_region_get_ram_ptr(&s->cppc_fastchan), 0,
           s->cppc_fastchan_size);
    memory_region_set_dirty(&s->cppc_fastchan, 0, s->cppc_fastchan_size);
    riscv_rpmi_cppc_reset_state(s);
}

bool riscv_rpmi_validate_cppc_config(RiscvRpmiState *s, Error **errp)
{
    uint64_t request_end;
    uint64_t feedback_end;
    uint64_t request_size = (uint64_t)s->hart_count *
                            sizeof(union rpmi_cppc_perf_request_fastchan);
    uint64_t feedback_size = (uint64_t)s->hart_count *
                             sizeof(struct rpmi_cppc_perf_feedback_fastchan);

    if (!riscv_rpmi_service_enabled(s, RISCV_RPMI_SERVICE_CPPC)) {
        return true;
    }

    if (!s->hart_count || !s->hart_ids) {
        error_setg(errp, "RPMI CPPC service requires hart IDs");
        return false;
    }

    if (!s->has_cppc_profile) {
        error_setg(errp, "RPMI CPPC service requires a CPPC profile");
        return false;
    }

    if (!s->cppc_profile.highest_perf || !s->cppc_profile.nominal_perf ||
        !s->cppc_profile.reference_perf) {
        error_setg(errp,
                   "RPMI CPPC profile performance values must be non-zero");
        return false;
    }

    if (s->cppc_profile.lowest_perf > s->cppc_profile.lowest_nonlinear_perf ||
        s->cppc_profile.lowest_nonlinear_perf > s->cppc_profile.nominal_perf ||
        s->cppc_profile.nominal_perf > s->cppc_profile.highest_perf) {
        error_setg(errp,
                   "RPMI CPPC profile performance values are inconsistent");
        return false;
    }

    if (!s->cppc_fastchan_size ||
        (s->cppc_fastchan_size & (s->cppc_fastchan_size - 1))) {
        error_setg(errp, "RPMI CPPC fast channel size must be a power of two");
        return false;
    }

    if ((s->cppc_fastchan_base & (RPMI_CPPC_FASTCHAN_SIZE - 1)) ||
        (s->cppc_perf_request_offset & (RPMI_CPPC_FASTCHAN_SIZE - 1)) ||
        (s->cppc_perf_feedback_offset & (RPMI_CPPC_FASTCHAN_SIZE - 1))) {
        error_setg(errp, "RPMI CPPC fast channel addresses must be aligned");
        return false;
    }

    request_end = s->cppc_perf_request_offset + request_size;
    feedback_end = s->cppc_perf_feedback_offset + feedback_size;
    if (request_end > s->cppc_fastchan_size ||
        feedback_end > s->cppc_fastchan_size) {
        error_setg(errp, "RPMI CPPC fast channel regions exceed memory size");
        return false;
    }

    if (s->cppc_perf_request_offset < feedback_end &&
        s->cppc_perf_feedback_offset < request_end) {
        error_setg(errp, "RPMI CPPC fast channel regions overlap");
        return false;
    }

    return true;
}
