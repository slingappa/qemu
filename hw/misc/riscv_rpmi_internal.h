/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI internal service helpers.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@qti.qualcomm.com>
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
bool riscv_rpmi_context_add_group(RiscvRpmiState *s,
                                  struct rpmi_service_group *group,
                                  const char *name,
                                  Error **errp);
void riscv_rpmi_context_remove_group(RiscvRpmiState *s,
                                     struct rpmi_service_group *group);

bool riscv_rpmi_sysreset_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_sysreset_remove(RiscvRpmiState *s);

bool riscv_rpmi_hsm_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_hsm_remove(RiscvRpmiState *s);
void riscv_rpmi_hsm_reset(RiscvRpmiState *s);
void riscv_rpmi_hsm_resume(RiscvRpmiState *s, uint32_t hart_index);

bool riscv_rpmi_syssusp_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_syssusp_remove(RiscvRpmiState *s);

void riscv_rpmi_cppc_configure(RiscvRpmiState *s,
                                const RiscvRpmiConfig *cfg);
bool riscv_rpmi_cppc_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_cppc_remove(RiscvRpmiState *s);
bool riscv_rpmi_cppc_realize_fastchan(RiscvRpmiState *s, DeviceState *dev,
                                      Error **errp);
void riscv_rpmi_cppc_unrealize_fastchan(RiscvRpmiState *s);
void riscv_rpmi_cppc_reset_fastchan(RiscvRpmiState *s);
bool riscv_rpmi_validate_cppc_config(RiscvRpmiState *s, Error **errp);

void riscv_rpmi_sysmsi_configure(RiscvRpmiState *s,
                                  const RiscvRpmiConfig *cfg);
bool riscv_rpmi_sysmsi_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_sysmsi_remove(RiscvRpmiState *s);

bool riscv_rpmi_clock_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_clock_remove(RiscvRpmiState *s);

void riscv_rpmi_mm_configure(RiscvRpmiState *s,
                              const RiscvRpmiConfig *cfg);
bool riscv_rpmi_mm_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_mm_remove(RiscvRpmiState *s);

bool riscv_rpmi_logging_add(RiscvRpmiState *s, Error **errp);
void riscv_rpmi_logging_remove(RiscvRpmiState *s);

#endif
