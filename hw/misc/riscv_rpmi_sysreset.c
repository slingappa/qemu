/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * RISC-V RPMI System Reset service.
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"
#include "qemu/log.h"
#include "librpmi_env.h"

static const rpmi_uint32_t rpmi_sysreset_types[] = {
    RPMI_SYSRST_TYPE_SHUTDOWN,
    RPMI_SYSRST_TYPE_COLD_REBOOT,
};

typedef struct RiscvRpmiSysresetGroup {
    struct rpmi_service_group group;
    struct rpmi_service services[RPMI_SYSRST_SRV_ID_MAX];
    RiscvRpmiState *rpmi;
} RiscvRpmiSysresetGroup;

static void riscv_rpmi_do_system_reset(RiscvRpmiState *s,
                                       rpmi_uint32_t reset_type)
{
    const RiscvRpmiMachineOps *ops = s->machine_ops;

    switch (reset_type) {
    case RPMI_SYSRST_TYPE_COLD_REBOOT:
    case RPMI_SYSRST_TYPE_WARM_REBOOT:
        if (ops && ops->system_reset) {
            ops->system_reset(s->machine_opaque);
        }
        break;
    case RPMI_SYSRST_TYPE_SHUTDOWN:
        if (ops && ops->system_shutdown) {
            ops->system_shutdown(s->machine_opaque);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unsupported reset type %u\n", __func__,
                      reset_type);
        break;
    }
}

static bool riscv_rpmi_sysreset_type_supported(uint32_t reset_type)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(rpmi_sysreset_types); i++) {
        if (rpmi_sysreset_types[i] == reset_type) {
            return true;
        }
    }

    return false;
}

static enum rpmi_error riscv_rpmi_sysreset_get_attributes(
    struct rpmi_service_group *group, struct rpmi_service *service,
    struct rpmi_transport *trans, rpmi_uint16_t request_data_len,
    const rpmi_uint8_t *request_data, rpmi_uint16_t *response_data_len,
    rpmi_uint8_t *response_data)
{
    uint32_t reset_type = ldl_le_p(request_data);
    uint32_t *resp = (uint32_t *)response_data;

    *response_data_len = 2 * sizeof(*resp);
    stl_le_p(&resp[0], RPMI_SUCCESS);
    stl_le_p(&resp[1], riscv_rpmi_sysreset_type_supported(reset_type) ?
             RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE : 0);

    return RPMI_SUCCESS;
}

static enum rpmi_error riscv_rpmi_sysreset_do_reset(
    struct rpmi_service_group *group, struct rpmi_service *service,
    struct rpmi_transport *trans, rpmi_uint16_t request_data_len,
    const rpmi_uint8_t *request_data, rpmi_uint16_t *response_data_len,
    rpmi_uint8_t *response_data)
{
    RiscvRpmiSysresetGroup *sysreset = group->priv;
    uint32_t reset_type = ldl_le_p(request_data);
    uint32_t *resp = (uint32_t *)response_data;

    *response_data_len = sizeof(*resp);

    if (!riscv_rpmi_sysreset_type_supported(reset_type)) {
        stl_le_p(resp, (uint32_t)RPMI_ERR_INVALID_PARAM);
        return RPMI_SUCCESS;
    }

    stl_le_p(resp, RPMI_SUCCESS);
    riscv_rpmi_do_system_reset(sysreset->rpmi, reset_type);
    return RPMI_SUCCESS;
}

struct rpmi_service_group *riscv_rpmi_sysreset_create(RiscvRpmiState *s)
{
    RiscvRpmiSysresetGroup *sysreset;
    struct rpmi_service_group *group;

    sysreset = g_new0(RiscvRpmiSysresetGroup, 1);
    sysreset->rpmi = s;

    sysreset->services[RPMI_SYSRST_SRV_ENABLE_NOTIFICATION] =
        (struct rpmi_service) {
            .service_id = RPMI_SYSRST_SRV_ENABLE_NOTIFICATION,
            .min_a2p_request_datalen = 8,
        };
    sysreset->services[RPMI_SYSRST_SRV_GET_ATTRIBUTES] =
        (struct rpmi_service) {
            .service_id = RPMI_SYSRST_SRV_GET_ATTRIBUTES,
            .min_a2p_request_datalen = 4,
            .process_a2p_request = riscv_rpmi_sysreset_get_attributes,
        };
    sysreset->services[RPMI_SYSRST_SRV_SYSTEM_RESET] =
        (struct rpmi_service) {
            .service_id = RPMI_SYSRST_SRV_SYSTEM_RESET,
            .min_a2p_request_datalen = 4,
            .process_a2p_request = riscv_rpmi_sysreset_do_reset,
        };

    group = &sysreset->group;
    group->name = "sysreset";
    group->servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET;
    group->max_service_id = RPMI_SYSRST_SRV_ID_MAX;
    group->servicegroup_version =
        RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
    group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
    group->services = sysreset->services;
    group->lock = rpmi_env_alloc_lock();
    group->priv = sysreset;

    return group;
}

void riscv_rpmi_sysreset_destroy(struct rpmi_service_group *group)
{
    if (!group) {
        return;
    }

    rpmi_env_free_lock(group->lock);
    g_free(group->priv);
}
