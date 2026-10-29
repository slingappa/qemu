/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI System MSI service.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"
#include "system/reset.h"
#include "system/runstate.h"

void riscv_rpmi_sysmsi_configure(RiscvRpmiState *s,
                                  const RiscvRpmiConfig *cfg)
{
    s->sysmsi_msi_base = cfg->sysmsi_msi_base;
    s->sysmsi_msi_size = cfg->sysmsi_msi_size;
}

static rpmi_bool_t riscv_rpmi_sysmsi_validate_addr(void *priv,
                                                     rpmi_uint64_t msi_addr)
{
    RiscvRpmiState *s = priv;

    if (!s->sysmsi_msi_size || msi_addr & 0x3) {
        return false;
    }

    return msi_addr >= s->sysmsi_msi_base &&
           msi_addr < s->sysmsi_msi_base + s->sysmsi_msi_size;
}

static rpmi_bool_t riscv_rpmi_sysmsi_mmode_preferred(void *priv,
                                                     rpmi_uint32_t msi_index)
{
    return false;
}

static void riscv_rpmi_sysmsi_get_name(void *priv, rpmi_uint32_t msi_index,
                                       char *out_name,
                                       rpmi_uint32_t out_name_sz)
{
    static const char * const names[RPMI_SYS_NUM_MSI] = {
        [RPMI_SYS_MSI_SHUTDOWN_INDEX] = "shutdown",
        [RPMI_SYS_MSI_REBOOT_INDEX] = "reboot",
        [RPMI_SYS_MSI_SUSPEND_INDEX] = "suspend",
        [RPMI_SYS_MSI_P2A_DB_INDEX] = "p2a-db",
    };

    if (msi_index < ARRAY_SIZE(names)) {
        g_strlcpy(out_name, names[msi_index], out_name_sz);
    }
}

static const struct rpmi_sysmsi_platform_ops riscv_rpmi_sysmsi_ops = {
    .validate_msi_addr = riscv_rpmi_sysmsi_validate_addr,
    .mmode_preferred = riscv_rpmi_sysmsi_mmode_preferred,
    .get_name = riscv_rpmi_sysmsi_get_name,
};

static void riscv_rpmi_powerdown_notify(Notifier *notifier, void *data)
{
    RiscvRpmiState *s = container_of(notifier, RiscvRpmiState,
                                     powerdown_notifier);

    if (s->sysmsi_group) {
        rpmi_service_group_sysmsi_inject(s->sysmsi_group,
                                         RPMI_SYS_MSI_SHUTDOWN_INDEX);
    }
}

static void riscv_rpmi_reset_notify(void *opaque)
{
    RiscvRpmiState *s = opaque;

    if (s->sysmsi_group) {
        rpmi_service_group_sysmsi_inject(s->sysmsi_group,
                                         RPMI_SYS_MSI_REBOOT_INDEX);
    }
}

static void riscv_rpmi_suspend_notify(Notifier *notifier, void *data)
{
    RiscvRpmiState *s = container_of(notifier, RiscvRpmiState,
                                     suspend_notifier);

    if (s->sysmsi_group) {
        rpmi_service_group_sysmsi_inject(s->sysmsi_group,
                                         RPMI_SYS_MSI_SUSPEND_INDEX);
    }
}

static bool riscv_rpmi_sysmsi_create(RiscvRpmiState *s,
                              struct rpmi_service_group **group,
                              Error **errp)
{
    *group = rpmi_service_group_sysmsi_create(RPMI_SYS_NUM_MSI,
                                              RPMI_SYS_MSI_P2A_DB_INDEX,
                                              &riscv_rpmi_sysmsi_ops, s);
    if (!*group) {
        error_setg(errp, "failed to create RPMI sysmsi service group");
        return false;
    }

    return true;
}

static void riscv_rpmi_sysmsi_attach(RiscvRpmiState *s,
                              struct rpmi_service_group *group)
{
    s->sysmsi_group = group;
    if (!s->powerdown_notifier_registered) {
        s->powerdown_notifier.notify = riscv_rpmi_powerdown_notify;
        qemu_register_powerdown_notifier(&s->powerdown_notifier);
        s->powerdown_notifier_registered = true;
    }
    if (!s->suspend_notifier_registered) {
        s->suspend_notifier.notify = riscv_rpmi_suspend_notify;
        qemu_register_suspend_notifier(&s->suspend_notifier);
        s->suspend_notifier_registered = true;
    }
    if (!s->reset_notifier_registered) {
        qemu_register_reset(riscv_rpmi_reset_notify, s);
        s->reset_notifier_registered = true;
    }
}

static void riscv_rpmi_sysmsi_destroy_group(struct rpmi_service_group *group)
{
    rpmi_service_group_sysmsi_destroy(group);
}

static void riscv_rpmi_sysmsi_destroy(RiscvRpmiState *s)
{
    if (s->powerdown_notifier_registered) {
        notifier_remove(&s->powerdown_notifier);
        s->powerdown_notifier_registered = false;
    }

    if (s->suspend_notifier_registered) {
        notifier_remove(&s->suspend_notifier);
        s->suspend_notifier_registered = false;
    }

    if (s->reset_notifier_registered) {
        qemu_unregister_reset(riscv_rpmi_reset_notify, s);
        s->reset_notifier_registered = false;
    }

    if (s->sysmsi_group) {
        rpmi_service_group_sysmsi_destroy(s->sysmsi_group);
        s->sysmsi_group = NULL;
    }
}

bool riscv_rpmi_sysmsi_add(RiscvRpmiState *s, Error **errp)
{
    struct rpmi_service_group *group;

    if (s->sysmsi_group) {
        error_setg(errp, "duplicate RPMI sysmsi service descriptor");
        return false;
    }

    if (!riscv_rpmi_sysmsi_create(s, &group, errp)) {
        return false;
    }

    if (!riscv_rpmi_context_add_group(s, group, "sysmsi", errp)) {
        riscv_rpmi_sysmsi_destroy_group(group);
        return false;
    }

    riscv_rpmi_sysmsi_attach(s, group);
    return true;
}

void riscv_rpmi_sysmsi_remove(RiscvRpmiState *s)
{
    riscv_rpmi_context_remove_group(s, s->sysmsi_group);
    riscv_rpmi_sysmsi_destroy(s);
}
