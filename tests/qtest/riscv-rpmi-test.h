/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Common helpers for RISC-V RPMI qtests.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#ifndef QTEST_RISCV_RPMI_TEST_H
#define QTEST_RISCV_RPMI_TEST_H

#include "libqtest.h"

#define RPMI_SHMEM_BASE 0x10200000ULL
#define RPMI_DOORBELL_BASE 0x10230000ULL
#define RVSERVER_RPMI_SHMEM_BASE 0x110000ULL
#define RVSERVER_RPMI_DOORBELL_BASE 0x140000ULL
#define RPMI_SLOT_SIZE 64

#define RPMI_A2P_HEAD rpmi_shmem_base
#define RPMI_A2P_TAIL (rpmi_shmem_base + RPMI_SLOT_SIZE)
#define RPMI_A2P_SLOT0 (rpmi_shmem_base + 2 * RPMI_SLOT_SIZE)

#define RPMI_SRVGRP_BASE 0x0001
#define RPMI_SRVGRP_SYSTEM_RESET 0x0003
#define RPMI_SRVGRP_SYSTEM_SUSPEND 0x0004
#define RPMI_SRVGRP_HSM 0x0005
#define RPMI_BASE_SRV_GET_PLATFORM_INFO 0x05
#define RPMI_BASE_SRV_PROBE_SERVICE_GROUP 0x06
#define RPMI_SYSRST_SRV_GET_ATTRIBUTES 0x02
#define RPMI_SYSRST_SRV_SYSTEM_RESET 0x03
#define RPMI_HSM_SRV_GET_HART_STATUS 0x02
#define RPMI_HSM_SRV_GET_HART_LIST 0x03
#define RPMI_HSM_SRV_GET_SUSPEND_TYPES 0x04
#define RPMI_HSM_SRV_GET_SUSPEND_INFO 0x05
#define RPMI_HSM_SRV_HART_START 0x06
#define RPMI_HSM_SRV_HART_STOP 0x07
#define RPMI_HSM_SRV_HART_SUSPEND 0x08
#define RPMI_SYSSUSP_SRV_GET_ATTRIBUTES 0x02
#define RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND 0x03
#define RPMI_MSG_NORMAL_REQUEST 0x00
#define RPMI_MSG_POSTED_REQUEST 0x01
#define RPMI_MSG_ACKNOWLEDGEMENT 0x02
#define RPMI_SYSRST_TYPE_SHUTDOWN 0x00
#define RPMI_SYSRST_TYPE_COLD_REBOOT 0x01
#define RPMI_SYSRST_TYPE_INVALID 0x03
#define RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE 1
#define RPMI_TOKEN 0x55aa
#define RPMI_ERR_NOTSUPP 0xfffffffeU
#define RPMI_ERR_INVALID_PARAM 0xfffffffdU
#define RPMI_ERR_INVALID_ADDR 0xfffffffbU
#define RPMI_ERR_DENIED 0xfffffffcU
#define RPMI_HSM_HART_STATE_STARTED 0x00
#define RPMI_HSM_HART_STATE_STOPPED 0x01
#define RPMI_HSM_HART_STATE_SUSPENDED 0x04
#define RPMI_HSM_TEST_START_ADDR 0x80000000ULL
#define RPMI_HSM_TEST_RESUME_ADDR 0x80001000ULL

#define RPMI_P2A_ACK_BASE (rpmi_shmem_base + 16 * RPMI_SLOT_SIZE)
#define RPMI_P2A_ACK_HEAD RPMI_P2A_ACK_BASE
#define RPMI_P2A_ACK_TAIL (RPMI_P2A_ACK_BASE + RPMI_SLOT_SIZE)
#define RPMI_P2A_ACK_SLOT0 (RPMI_P2A_ACK_BASE + 2 * RPMI_SLOT_SIZE)

typedef struct RiscvRpmiQTestMachine {
    const char *machine_opts;
    const char *service_group_extra_args;
    uint64_t shmem_base;
    uint64_t doorbell_base;
    const char *platform_info;
} RiscvRpmiQTestMachine;

typedef enum RiscvRpmiQTestCoverage {
    RPMI_QTEST_BASE = 1 << 0,
    RPMI_QTEST_SYSRESET = 1 << 1,
    RPMI_QTEST_RESET = 1 << 2,
    RPMI_QTEST_NEGATIVE = 1 << 3,
    RPMI_QTEST_MIGRATION = 1 << 4,
    RPMI_QTEST_HSM = 1 << 5,
    RPMI_QTEST_HSM_MULTI_SOCKET = 1 << 6,
    RPMI_QTEST_SYSSUSP = 1 << 7,
    RPMI_QTEST_REPEATED_RESET = 1 << 8,
} RiscvRpmiQTestCoverage;

#define RPMI_QTEST_ALL \
    (RPMI_QTEST_BASE | RPMI_QTEST_SYSRESET | RPMI_QTEST_RESET | \
     RPMI_QTEST_NEGATIVE | RPMI_QTEST_MIGRATION | RPMI_QTEST_HSM | \
     RPMI_QTEST_HSM_MULTI_SOCKET | RPMI_QTEST_SYSSUSP | \
     RPMI_QTEST_REPEATED_RESET)

#define RPMI_QTEST_SERVER_REF \
    (RPMI_QTEST_BASE | RPMI_QTEST_SYSRESET | RPMI_QTEST_RESET | \
     RPMI_QTEST_NEGATIVE | RPMI_QTEST_MIGRATION | RPMI_QTEST_HSM | \
     RPMI_QTEST_SYSSUSP)

extern const RiscvRpmiQTestMachine virt_rpmi_machine;
extern const RiscvRpmiQTestMachine rvserver_ref_rpmi_machine;

extern uint64_t rpmi_shmem_base;
extern uint64_t rpmi_doorbell_base;

QTestState *rpmi_qtest_init(const RiscvRpmiQTestMachine *machine,
                            const char *extra_args);
void rpmi_send_request(QTestState *qts, uint16_t service_group,
                       uint8_t service_id, uint8_t request_type,
                       const uint32_t *data, size_t data_words);
uint32_t rpmi_response_word(QTestState *qts, unsigned int word);
void rpmi_expect_ack(QTestState *qts, uint16_t service_group,
                     uint8_t service_id, uint16_t data_len);
void rpmi_send_sysreset(QTestState *qts, uint32_t reset_type,
                        uint8_t request_type);
void rpmi_expect_qemu_failure(const char *extra_args,
                              const char *stderr_needle);
void rpmi_check_platform_info(QTestState *qts, const char *expected);
void rpmi_test_platform_info(const RiscvRpmiQTestMachine *machine);
void rpmi_probe_group(QTestState *qts, uint32_t service_group, bool present);
void rpmi_check_service_groups(QTestState *qts);
void rpmi_test_service_groups(const RiscvRpmiQTestMachine *machine,
                              const char *extra_args);
void rpmi_check_hsm_hart_list(QTestState *qts, uint32_t hart_count);
void rpmi_test_hsm_hart_list(const RiscvRpmiQTestMachine *machine,
                             const char *extra_args);
void rpmi_expect_hsm_status(QTestState *qts, uint32_t hart_id,
                            uint32_t expected_state);
uint64_t rpmi_hart_pc(QTestState *qts, uint32_t cpu_index);
void rpmi_register_common_tests(const char *prefix,
                                const RiscvRpmiQTestMachine *machine,
                                unsigned int coverage);

#endif
