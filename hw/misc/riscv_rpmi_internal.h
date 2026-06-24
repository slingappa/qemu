/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * RISC-V RPMI internal service helpers.
 */

#ifndef HW_MISC_RISCV_RPMI_INTERNAL_H
#define HW_MISC_RISCV_RPMI_INTERNAL_H

#include "qapi/error.h"
#include "hw/misc/riscv_rpmi.h"
#include "librpmi.h"

#define RPMI_PLAT_INFO "QEMU RISC-V RPMI"
#define RPMI_CPPC_FREQ_MHZ_TO_HZ 1000000ULL
#define RISCV_RPMI_CLOCK_COUNT 6
#define RISCV_RPMI_MM_MAX_VARIABLES 100
#define RISCV_RPMI_MM_STORE_VERSION 1

struct RiscvRpmiMmVariable {
    bool valid;
    struct rpmi_guid_t guid;
    uint32_t attr;
    uint64_t namesize;
    uint64_t datasize;
    uint8_t *name;
    uint8_t *data;
};

extern const struct rpmi_shmem_platform_ops rpmi_shmem_qemu_ops;
bool riscv_rpmi_service_enabled(RiscvRpmiState *s,
                                RiscvRpmiServiceKind kind);

struct rpmi_service_group *riscv_rpmi_sysreset_create(RiscvRpmiState *s);
void riscv_rpmi_sysreset_destroy(struct rpmi_service_group *group);
bool riscv_rpmi_hsm_create(RiscvRpmiState *s,
                           struct rpmi_service_group **group,
                           Error **errp);
void riscv_rpmi_hsm_destroy(RiscvRpmiState *s);
bool riscv_rpmi_syssusp_create(RiscvRpmiState *s,
                               struct rpmi_service_group **group,
                               Error **errp);
void riscv_rpmi_syssusp_destroy(RiscvRpmiState *s);
bool riscv_rpmi_cppc_create(RiscvRpmiState *s,
                            struct rpmi_service_group **group,
                            Error **errp);
void riscv_rpmi_cppc_destroy(RiscvRpmiState *s);
void riscv_rpmi_cppc_reset_state(RiscvRpmiState *s);
bool riscv_rpmi_validate_cppc_config(RiscvRpmiState *s, Error **errp);
bool riscv_rpmi_clock_create(RiscvRpmiState *s,
                             struct rpmi_service_group **group,
                             Error **errp);
void riscv_rpmi_clock_destroy(RiscvRpmiState *s);
bool riscv_rpmi_mm_create(RiscvRpmiState *s,
                          struct rpmi_service_group **group,
                          Error **errp);
void riscv_rpmi_mm_destroy(RiscvRpmiState *s);
bool riscv_rpmi_sysmsi_create(RiscvRpmiState *s,
                              struct rpmi_service_group **group,
                              Error **errp);
void riscv_rpmi_sysmsi_attach(RiscvRpmiState *s,
                              struct rpmi_service_group *group);
void riscv_rpmi_sysmsi_destroy_group(struct rpmi_service_group *group);
void riscv_rpmi_sysmsi_destroy(RiscvRpmiState *s);
#ifdef CONFIG_LIBRPMI_LOGGING
bool riscv_rpmi_logging_create(RiscvRpmiState *s,
                               struct rpmi_service_group **group,
                               Error **errp);
void riscv_rpmi_logging_destroy(RiscvRpmiState *s);
#endif

#endif
