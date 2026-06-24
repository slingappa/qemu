/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * RISC-V RPMI Logging service.
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"

#ifdef CONFIG_LIBRPMI_LOGGING
static enum rpmi_error riscv_rpmi_logging_set_state(
    void *priv, rpmi_uint32_t log_type, rpmi_uint32_t datalen_bytes,
    const void *data)
{
    RiscvRpmiState *s = priv;

    if (datalen_bytes > sizeof(s->logging_data)) {
        return RPMI_ERR_INVALID_PARAM;
    }

    s->logging_type = log_type;
    s->logging_data_len = datalen_bytes;
    memset(s->logging_data, 0, sizeof(s->logging_data));
    if (datalen_bytes) {
        memcpy(s->logging_data, data, datalen_bytes);
    }

    return RPMI_SUCCESS;
}

static const struct rpmi_logging_platform_ops riscv_rpmi_logging_ops = {
    .do_set_state = riscv_rpmi_logging_set_state,
};

bool riscv_rpmi_logging_create(RiscvRpmiState *s,
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

void riscv_rpmi_logging_destroy(RiscvRpmiState *s)
{
    if (s->logging_group) {
        rpmi_service_group_logging_destroy(s->logging_group);
        s->logging_group = NULL;
    }
}
#endif
