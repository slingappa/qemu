/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI definitions shared by RPMI transport and FDT helpers.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#ifndef HW_MISC_RISCV_RPMI_H
#define HW_MISC_RISCV_RPMI_H

#include "exec/hwaddr.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"
#include "qemu/notify.h"

#ifdef CONFIG_LIBRPMI
#include "librpmi.h"
#else
enum rpmi_servicegroup_id {
    RPMI_SRVGRP_SYSTEM_MSI = 0x0002,
    RPMI_SRVGRP_SYSTEM_RESET = 0x0003,
    RPMI_SRVGRP_SYSTEM_SUSPEND = 0x0004,
    RPMI_SRVGRP_HSM = 0x0005,
    RPMI_SRVGRP_CPPC = 0x0006,
    RPMI_SRVGRP_CLOCK = 0x0008,
    RPMI_SRVGRP_MANAGEMENT_MODE = 0x000b,
    RPMI_SRVGRP_LOGGING = 0x000e,
};
#endif

#define RPMI_QUEUE_SLOT_SIZE 64
#define RPMI_DBREG_SIZE      0x1000

#define RPMI_ALL_NUM_QUEUES 4
#define RPMI_A2P_NUM_QUEUES 2
#define RPMI_ALL_NUM_REGS   (RPMI_ALL_NUM_QUEUES + 1)
#define RPMI_A2P_NUM_REGS   (RPMI_A2P_NUM_QUEUES + 1)

#define VIRT_RPMI_A2P_REQ_SIZE (16 * RPMI_QUEUE_SLOT_SIZE)
#define VIRT_RPMI_P2A_REQ_SIZE 0

#define RPMI_SYS_MSI_SHUTDOWN_INDEX 0
#define RPMI_SYS_MSI_REBOOT_INDEX   1
#define RPMI_SYS_MSI_SUSPEND_INDEX  2
#define RPMI_SYS_MSI_P2A_DB_INDEX   3
#define RPMI_SYS_NUM_MSI            4
#define RISCV_RPMI_LOGGING_DATA_MAX 64
#define TYPE_RISCV_RPMI "riscv-rpmi"
OBJECT_DECLARE_SIMPLE_TYPE(RiscvRpmiState, RISCV_RPMI)

struct rpmi_context;
struct rpmi_service_group;
struct rpmi_shmem;
struct rpmi_transport;
struct rpmi_cppc_regs;
struct rpmi_hsm;
typedef struct RiscvRpmiMmVariable RiscvRpmiMmVariable;

typedef struct RiscvRpmiCppcProfile {
    uint32_t highest_perf;
    uint32_t nominal_perf;
    uint32_t lowest_nonlinear_perf;
    uint32_t lowest_perf;
    uint32_t reference_perf;
    uint32_t lowest_freq;
    uint32_t nominal_freq;
    uint32_t transition_latency;
} RiscvRpmiCppcProfile;

typedef struct RiscvRpmiClockState {
    uint32_t state;
    uint64_t rate;
} RiscvRpmiClockState;
typedef struct RiscvRpmiMachineOps {
    void (*system_reset)(void);
    void (*system_shutdown)(void);
    void (*system_suspend)(void);
    void (*register_wakeup_support)(void);
    bool (*system_can_resume)(void);
} RiscvRpmiMachineOps;

typedef struct RiscvRpmiServiceConfig {
    const char *node_name;
    const char *compatible;
    enum rpmi_servicegroup_id service_group;
    bool has_mpxy_channel;
    uint32_t mpxy_channel;
} RiscvRpmiServiceConfig;

typedef struct RiscvRpmiConfig {
    hwaddr doorbell_base;
    hwaddr shmem_base;
    hwaddr shmem_size;
    uint32_t a2p_req_size;
    uint32_t p2a_req_size;
    const char *platform_info;
    BlockBackend *mm_store_blk;
    uint64_t mm_store_offset;
    uint64_t mm_store_size;
    const RiscvRpmiMachineOps *machine_ops;
    hwaddr sysmsi_msi_base;
    hwaddr sysmsi_msi_size;
    hwaddr cppc_fastchan_base;
    hwaddr cppc_fastchan_size;
    uint64_t cppc_perf_request_offset;
    uint64_t cppc_perf_feedback_offset;
    const RiscvRpmiCppcProfile *cppc_profile;
    const uint32_t *hart_ids;
    uint32_t hart_count;
    const RiscvRpmiServiceConfig *services;
    uint32_t service_count;
} RiscvRpmiConfig;

struct RiscvRpmiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    MemoryRegion shmem;
    uint64_t shmem_base;
    uint64_t shmem_size;
    uint32_t a2p_req_size;
    uint32_t p2a_req_size;
    char *platform_info;
    BlockBackend *mm_store_blk;
    uint64_t mm_store_offset;
    uint64_t mm_store_size;
    const RiscvRpmiMachineOps *machine_ops;
    hwaddr sysmsi_msi_base;
    hwaddr sysmsi_msi_size;
    hwaddr cppc_fastchan_base;
    hwaddr cppc_fastchan_size;
    uint64_t cppc_perf_request_offset;
    uint64_t cppc_perf_feedback_offset;
    RiscvRpmiCppcProfile cppc_profile;
    bool has_cppc_profile;
    MemoryRegion cppc_fastchan;
    struct rpmi_shmem *cppc_fastchan_shmem;
    struct rpmi_service_group *sysreset_group;
    struct rpmi_service_group *syssusp_group;
    struct rpmi_hsm *hsm;
    struct rpmi_service_group *hsm_group;
    uint32_t *hsm_hw_states;
    struct rpmi_cppc_regs *cppc_regs;
    uint32_t *cppc_desired_perf;
    int64_t cppc_counter_base_ns;
    struct rpmi_service_group *cppc_group;
    struct rpmi_service_group *sysmsi_group;
    RiscvRpmiClockState *clock_state;
    struct rpmi_service_group *clock_group;
    struct rpmi_service_group *mm_group;
    RiscvRpmiMmVariable *mm_variables;
    uint32_t mm_variable_count;
    struct rpmi_service_group *logging_group;
    uint32_t logging_type;
    uint32_t logging_data_len;
    uint8_t logging_data[RISCV_RPMI_LOGGING_DATA_MAX];
    Notifier powerdown_notifier;
    Notifier suspend_notifier;
    bool powerdown_notifier_registered;
    bool suspend_notifier_registered;
    bool reset_notifier_registered;
    Notifier wakeup_notifier;
    bool wakeup_notifier_registered;
    QEMUTimer *wakeup_timer;
    bool syssusp_resume_pending;
    uint32_t syssusp_resume_hart_index;
    uint64_t syssusp_resume_addr;
    bool has_cppc_fastchan;
    uint32_t *hart_ids;
    uint32_t hart_count;
    const RiscvRpmiServiceConfig *services;
    uint32_t service_count;
    uint32_t doorbell;
    struct rpmi_shmem *rpmi_shmem;
    struct rpmi_transport *transport;
    struct rpmi_context *context;
    bool has_shmem;
};

DeviceState *riscv_rpmi_create(const RiscvRpmiConfig *cfg, Error **errp);

#endif
