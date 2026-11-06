/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI Logging service.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"

static enum rpmi_error riscv_rpmi_logging_log_data(
    void *priv, rpmi_uint32_t log_type, rpmi_uint32_t num_words,
    const rpmi_uint32_t *words)
{
    RiscvRpmiState *s = priv;
    size_t data_len = num_words * sizeof(*words);

    if (num_words > sizeof(s->logging_data) / sizeof(*words)) {
        return RPMI_ERR_INVALID_PARAM;
    }

    s->logging_type = log_type;
    s->logging_data_len = data_len;
    memset(s->logging_data, 0, sizeof(s->logging_data));
    if (data_len) {
        memcpy(s->logging_data, words, data_len);
    }

    return RPMI_SUCCESS;
}

static const struct rpmi_logging_platform_ops riscv_rpmi_logging_ops = {
    .log_data = riscv_rpmi_logging_log_data,
};

static bool riscv_rpmi_logging_create(RiscvRpmiState *s,
                               struct rpmi_service_group **group,
                               Error **errp)
{
    *group = rpmi_service_group_logging_create(&riscv_rpmi_logging_ops, s);
    if (!*group) {
        error_setg(errp, "failed to create RPMI logging service group");
        return false;
    }

    return true;
}

static void riscv_rpmi_logging_destroy(RiscvRpmiState *s)
{
    if (s->logging_group) {
        rpmi_service_group_logging_destroy(s->logging_group);
        s->logging_group = NULL;
    }
}

bool riscv_rpmi_logging_add(RiscvRpmiState *s, Error **errp)
{
    struct rpmi_service_group *group;

    if (s->logging_group) {
        error_setg(errp, "duplicate RPMI logging service descriptor");
        return false;
    }

    if (!riscv_rpmi_logging_create(s, &group, errp)) {
        return false;
    }

    s->logging_group = group;
    if (!riscv_rpmi_context_add_group(s, group, "logging", errp)) {
        riscv_rpmi_logging_destroy(s);
        return false;
    }

    return true;
}

void riscv_rpmi_logging_remove(RiscvRpmiState *s)
{
    riscv_rpmi_context_remove_group(s, s->logging_group);
    riscv_rpmi_logging_destroy(s);
}
