/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Common helpers for RISC-V RPMI qtests.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv-rpmi-test.h"
#include "qobject/qdict.h"

const RiscvRpmiQTestMachine virt_rpmi_machine = {
    .machine_opts = "virt,rpmi=on",
    .service_group_extra_args = ",aia=aplic-imsic",
    .shmem_base = RPMI_SHMEM_BASE,
    .doorbell_base = RPMI_DOORBELL_BASE,
    .platform_info = "QEMU RISC-V virt RPMI",
};

const RiscvRpmiQTestMachine rvserver_ref_rpmi_machine = {
    .machine_opts = "riscv-server-ref",
    .service_group_extra_args = "",
    .shmem_base = RVSERVER_RPMI_SHMEM_BASE,
    .doorbell_base = RVSERVER_RPMI_DOORBELL_BASE,
    .platform_info = "QEMU RISC-V server-ref RPMI",
};

uint64_t rpmi_shmem_base = RPMI_SHMEM_BASE;
uint64_t rpmi_doorbell_base = RPMI_DOORBELL_BASE;
static uint64_t rpmi_response_base;

QTestState *rpmi_qtest_init(const RiscvRpmiQTestMachine *machine,
                            const char *extra_args)
{
    rpmi_shmem_base = machine->shmem_base;
    rpmi_doorbell_base = machine->doorbell_base;

    return qtest_initf("-machine %s%s", machine->machine_opts, extra_args);
}

static uint64_t rpmi_queue_slot(uint64_t queue_base, uint32_t index)
{
    return queue_base + (index + 2) * RPMI_SLOT_SIZE;
}

void rpmi_send_request(QTestState *qts, uint16_t service_group,
                       uint8_t service_id, uint8_t request_type,
                       const uint32_t *data, size_t data_words)
{
    uint32_t tail = qtest_readl(qts, RPMI_A2P_TAIL);
    uint64_t slot = rpmi_queue_slot(rpmi_shmem_base, tail);
    size_t i;

    qtest_writew(qts, slot, service_group);
    qtest_writeb(qts, slot + 2, service_id);
    qtest_writeb(qts, slot + 3, request_type);
    qtest_writew(qts, slot + 4, data_words * sizeof(*data));
    qtest_writew(qts, slot + 6, RPMI_TOKEN);

    for (i = 0; i < data_words; i++) {
        qtest_writel(qts, slot + 8 + i * sizeof(*data), data[i]);
    }

    g_test_message(
        "RPMI_A2P_REQ shmem=0x%016" PRIx64 " doorbell=0x%016" PRIx64
        " group=0x%04x service=0x%02x type=0x%02x data_len=%zu"
        " token=0x%04x a2p_tail=%u slot=0x%016" PRIx64,
        (uint64_t)rpmi_shmem_base, (uint64_t)rpmi_doorbell_base,
        service_group, service_id, request_type, data_words * sizeof(*data),
        RPMI_TOKEN, tail, slot);

    qtest_writel(qts, RPMI_A2P_TAIL, (tail + 1) % 16);
    qtest_writel(qts, rpmi_doorbell_base, 1);
}

uint32_t rpmi_response_word(QTestState *qts, unsigned int word)
{
    return qtest_readl(qts, rpmi_response_base + 8 + word * sizeof(uint32_t));
}

void rpmi_expect_ack(QTestState *qts, uint16_t service_group,
                     uint8_t service_id, uint16_t data_len)
{
    uint32_t head = qtest_readl(qts, RPMI_P2A_ACK_HEAD);
    uint32_t tail = qtest_readl(qts, RPMI_P2A_ACK_TAIL);

    g_assert_cmphex(tail, !=, head);
    rpmi_response_base = rpmi_queue_slot(RPMI_P2A_ACK_BASE, head);
    g_assert_cmphex(qtest_readw(qts, rpmi_response_base), ==, service_group);
    g_assert_cmphex(qtest_readb(qts, rpmi_response_base + 2), ==,
                    service_id);
    g_assert_cmphex(qtest_readb(qts, rpmi_response_base + 3), ==,
                    RPMI_MSG_ACKNOWLEDGEMENT);
    g_assert_cmphex(qtest_readw(qts, rpmi_response_base + 4), ==, data_len);
    g_assert_cmphex(qtest_readw(qts, rpmi_response_base + 6), ==,
                    RPMI_TOKEN);
    g_test_message(
        "RPMI_P2A_ACK shmem=0x%016" PRIx64
        " group=0x%04x service=0x%02x type=0x%02x data_len=%u"
        " token=0x%04x p2a_head=%u slot=0x%016" PRIx64
        " status=0x%08x",
        (uint64_t)rpmi_shmem_base, service_group, service_id,
        RPMI_MSG_ACKNOWLEDGEMENT, data_len, RPMI_TOKEN, head,
        rpmi_response_base,
        data_len >= sizeof(uint32_t) ? rpmi_response_word(qts, 0) : 0);
    qtest_writel(qts, RPMI_P2A_ACK_HEAD, (head + 1) % 16);
}

void rpmi_send_sysreset(QTestState *qts, uint32_t reset_type,
                        uint8_t request_type)
{
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                      RPMI_SYSRST_SRV_SYSTEM_RESET, request_type,
                      &reset_type, 1);
}

void rpmi_expect_qemu_failure(const char *extra_args,
                              const char *stderr_needle)
{
    g_autoptr(GError) error = NULL;
    g_auto(GStrv) split_args = NULL;
    g_autoptr(GPtrArray) argv = g_ptr_array_new();
    g_autofree char *stderr_data = NULL;
    gint wait_status;
    gboolean spawned;

    g_assert_true(g_shell_parse_argv(extra_args, NULL, &split_args, &error));
    g_assert_no_error(error);

    g_ptr_array_add(argv, (gpointer)qtest_qemu_binary(NULL));
    for (char **arg = split_args; *arg; arg++) {
        g_ptr_array_add(argv, *arg);
    }
    g_ptr_array_add(argv, NULL);

    spawned = g_spawn_sync(NULL, (char **)argv->pdata, NULL,
                           G_SPAWN_STDOUT_TO_DEV_NULL,
                           NULL, NULL, NULL, &stderr_data,
                           &wait_status, &error);
    g_assert_true(spawned);
    g_assert_no_error(error);
    g_assert_false(g_spawn_check_exit_status(wait_status, NULL));
    g_assert_nonnull(stderr_data);
    g_assert_nonnull(strstr(stderr_data, stderr_needle));
}

void rpmi_check_platform_info(QTestState *qts, const char *expected)
{
    size_t expected_len = strlen(expected) + 1;
    size_t i;

    rpmi_send_request(qts, RPMI_SRVGRP_BASE,
                      RPMI_BASE_SRV_GET_PLATFORM_INFO,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);

    rpmi_expect_ack(qts, RPMI_SRVGRP_BASE,
                    RPMI_BASE_SRV_GET_PLATFORM_INFO,
                    2 * sizeof(uint32_t) + expected_len);
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, expected_len);
    for (i = 0; i < expected_len; i++) {
        g_assert_cmphex(qtest_readb(qts, RPMI_P2A_ACK_SLOT0 + 16 + i), ==,
                        expected[i]);
    }
}

void rpmi_test_platform_info(const RiscvRpmiQTestMachine *machine)
{
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    rpmi_check_platform_info(qts, machine->platform_info);

    qtest_quit(qts);
}

void rpmi_probe_group(QTestState *qts, uint32_t service_group, bool present)
{
    rpmi_send_request(qts, RPMI_SRVGRP_BASE,
                      RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
                      RPMI_MSG_NORMAL_REQUEST, &service_group, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_BASE,
                    RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    if (present) {
        g_assert_cmphex(rpmi_response_word(qts, 1), !=, 0);
    } else {
        g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    }
}

void rpmi_check_service_groups(QTestState *qts)
{
    rpmi_probe_group(qts, RPMI_SRVGRP_BASE, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_RESET, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_HSM, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_SUSPEND, true);
}

void rpmi_test_service_groups(const RiscvRpmiQTestMachine *machine,
                              const char *extra_args)
{
    QTestState *qts;

    qts = rpmi_qtest_init(machine, extra_args);
    rpmi_check_service_groups(qts);

    qtest_quit(qts);
}

void rpmi_check_hsm_hart_list(QTestState *qts, uint32_t hart_count)
{
    uint32_t start_index = 0;

    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                      RPMI_MSG_NORMAL_REQUEST, &start_index, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                    (3 + hart_count) * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, hart_count);
    for (uint32_t i = 0; i < hart_count; i++) {
        g_assert_cmphex(rpmi_response_word(qts, i + 3), ==, i);
    }
}

void rpmi_test_hsm_hart_list(const RiscvRpmiQTestMachine *machine,
                             const char *extra_args)
{
    QTestState *qts;

    qts = rpmi_qtest_init(machine, extra_args);
    rpmi_check_hsm_hart_list(qts, 4);

    qtest_quit(qts);
}

void rpmi_expect_hsm_status(QTestState *qts, uint32_t hart_id,
                            uint32_t expected_state)
{
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                      RPMI_MSG_NORMAL_REQUEST, &hart_id, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, expected_state);
}

uint64_t rpmi_hart_pc(QTestState *qts, uint32_t cpu_index)
{
    g_autofree char *registers = qtest_hmp(qts, "info registers %u",
                                           cpu_index);
    const char *pc_line;
    uint64_t pc;

    pc_line = strstr(registers, "\n pc");
    g_assert_nonnull(pc_line);
    g_assert_cmpint(sscanf(pc_line, "\n pc %" SCNx64, &pc), ==, 1);

    return pc;
}

static void rpmi_add_common_test(const char *prefix, const char *name,
                                 const RiscvRpmiQTestMachine *machine,
                                 void (*fn)(const void *))
{
    g_autofree char *path = g_strdup_printf("%s/%s", prefix, name);

    qtest_add_data_func(path, machine, fn);
}

static void test_rpmi_common_base_platform_info(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;

    rpmi_test_platform_info(machine);
}

static void test_rpmi_common_base_probe_service_groups(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;

    rpmi_test_service_groups(machine, machine->service_group_extra_args);
}

static void test_rpmi_common_sysreset_attrs(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    uint32_t reset_type = RPMI_SYSRST_TYPE_SHUTDOWN;

    qts = rpmi_qtest_init(machine, "");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                      RPMI_SYSRST_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &reset_type, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_GET_ATTRIBUTES, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE);

    qtest_system_reset(qts);
    reset_type = RPMI_SYSRST_TYPE_COLD_REBOOT;
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                      RPMI_SYSRST_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &reset_type, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_GET_ATTRIBUTES, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE);

    qtest_system_reset(qts);
    reset_type = RPMI_SYSRST_TYPE_INVALID;
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                      RPMI_SYSRST_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &reset_type, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_GET_ATTRIBUTES, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_common_sysreset_shutdown(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_SHUTDOWN,
                       RPMI_MSG_POSTED_REQUEST);
    qtest_qmp_eventwait(qts, "SHUTDOWN");
    qtest_quit(qts);
}

static void test_rpmi_common_sysreset_cold_reboot(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, " -no-reboot");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_COLD_REBOOT,
                       RPMI_MSG_POSTED_REQUEST);
    qtest_qmp_eventwait(qts, "SHUTDOWN");
    qtest_quit(qts);
}

static void test_rpmi_common_sysreset_invalid_type(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_INVALID,
                       RPMI_MSG_NORMAL_REQUEST);

    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_SYSTEM_RESET, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_PARAM);

    qtest_quit(qts);
}

static void test_rpmi_common_repeated_reset_after_traffic(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    uint32_t reset_type = RPMI_SYSRST_TYPE_SHUTDOWN;

    qts = rpmi_qtest_init(machine, machine->service_group_extra_args);
    for (uint32_t i = 0; i < 5; i++) {
        rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                          RPMI_SYSRST_SRV_GET_ATTRIBUTES,
                          RPMI_MSG_NORMAL_REQUEST,
                          &reset_type, 1);
        rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                        RPMI_SYSRST_SRV_GET_ATTRIBUTES,
                        2 * sizeof(uint32_t));
        g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
        qtest_system_reset(qts);
    }
    qtest_quit(qts);
}

static void test_rpmi_common_reset_clears_transport(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_INVALID,
                       RPMI_MSG_NORMAL_REQUEST);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_SYSTEM_RESET, sizeof(uint32_t));

    qtest_system_reset(qts);

    g_assert_cmphex(qtest_readl(qts, RPMI_A2P_TAIL), ==, 0);
    g_assert_cmphex(qtest_readl(qts, RPMI_P2A_ACK_TAIL), ==, 0);
    g_assert_cmphex(qtest_readl(qts, rpmi_doorbell_base), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_common_doorbell_invalid_access(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    qtest_writeb(qts, rpmi_doorbell_base, 1);
    qtest_writel(qts, rpmi_doorbell_base + 4, 1);
    g_assert_cmphex(qtest_readl(qts, rpmi_doorbell_base), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_common_queue_bounds(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine, "");
    qtest_writel(qts, RPMI_A2P_TAIL, 0x1000);
    qtest_writel(qts, rpmi_doorbell_base, 1);
    g_assert_cmphex(qtest_readl(qts, RPMI_A2P_HEAD), ==, 0);
    g_assert_cmphex(qtest_readl(qts, RPMI_P2A_ACK_TAIL), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_common_migration_blocked(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    QDict *error;
    const char *desc;

    qts = rpmi_qtest_init(machine, " -S");
    error = qtest_qmp_assert_failure_ref(qts,
        "{ 'execute': 'migrate',"
        "  'arguments': { 'uri': 'exec:cat > /dev/null' } }");
    desc = qdict_get_try_str(error, "desc");

    g_assert_nonnull(desc);
    g_assert_nonnull(strstr(desc, "non-migratable device"));
    g_assert_nonnull(strstr(desc, "riscv-rpmi"));

    qobject_unref(error);
    qtest_quit(qts);
}

static void test_rpmi_common_hsm_hart_list(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;

    rpmi_test_hsm_hart_list(machine, " -smp 4");
}

static void test_rpmi_common_hsm_multi_socket_hart_list(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;

    qts = rpmi_qtest_init(machine,
                          " -smp 4,sockets=2,cores=2,threads=1");
    rpmi_check_hsm_hart_list(qts, 4);

    qtest_quit(qts);
}

static void test_rpmi_common_hsm_hart_status(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    uint32_t hart_id = 3;

    qts = rpmi_qtest_init(machine, " -smp 4");
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                      RPMI_MSG_NORMAL_REQUEST, &hart_id, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_HSM_HART_STATE_STARTED);

    qtest_quit(qts);
}

static void test_rpmi_common_hsm_hart_control(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    uint32_t hart_id = 1;
    uint32_t stop_request[] = { hart_id };
    uint32_t start_request[] = { hart_id, RPMI_HSM_TEST_START_ADDR, 0 };
    uint32_t suspend_request[] = { hart_id, 0, RPMI_HSM_TEST_RESUME_ADDR, 0 };
    uint32_t start_index = 0;
    uint32_t suspend_type = 0;

    qts = rpmi_qtest_init(machine, " -smp 2");
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_SUSPEND_TYPES,
                      RPMI_MSG_NORMAL_REQUEST, &start_index, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_SUSPEND_TYPES,
                    4 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 1);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_SUSPEND_INFO,
                      RPMI_MSG_NORMAL_REQUEST, &suspend_type, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_SUSPEND_INFO,
                    6 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_STOP,
                      RPMI_MSG_NORMAL_REQUEST, stop_request,
                      ARRAY_SIZE(stop_request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_STOP,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    rpmi_expect_hsm_status(qts, hart_id, RPMI_HSM_HART_STATE_STOPPED);

    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_START,
                      RPMI_MSG_NORMAL_REQUEST, start_request,
                      ARRAY_SIZE(start_request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_START,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    rpmi_expect_hsm_status(qts, hart_id, RPMI_HSM_HART_STATE_STARTED);
    g_assert_cmphex(rpmi_hart_pc(qts, hart_id), ==, RPMI_HSM_TEST_START_ADDR);

    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_SUSPEND,
                      RPMI_MSG_NORMAL_REQUEST, suspend_request,
                      ARRAY_SIZE(suspend_request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_SUSPEND,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    rpmi_expect_hsm_status(qts, hart_id, RPMI_HSM_HART_STATE_SUSPENDED);

    qtest_quit(qts);
}

static void test_rpmi_common_syssusp_attrs_and_suspend(const void *opaque)
{
    const RiscvRpmiQTestMachine *machine = opaque;
    QTestState *qts;
    uint32_t suspend_type = 0;
    uint32_t suspend_request[] = { 0, 0, 0x80000000, 0 };

    qts = rpmi_qtest_init(machine, " -smp 1");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_SUSPEND,
                      RPMI_SYSSUSP_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &suspend_type, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_SUSPEND,
                    RPMI_SYSSUSP_SRV_GET_ATTRIBUTES,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 3);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_SUSPEND,
                      RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
                      RPMI_MSG_NORMAL_REQUEST, suspend_request,
                      ARRAY_SIZE(suspend_request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_SUSPEND,
                    RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    qtest_qmp_eventwait(qts, "SUSPEND");
    qtest_qmp_assert_success(qts, "{ 'execute': 'system_wakeup' }");
    qtest_qmp_eventwait(qts, "WAKEUP");

    qtest_quit(qts);
}

void rpmi_register_common_tests(const char *prefix,
                                const RiscvRpmiQTestMachine *machine,
                                unsigned int coverage)
{
    if (coverage & RPMI_QTEST_BASE) {
        rpmi_add_common_test(prefix, "base/platform-info", machine,
                             test_rpmi_common_base_platform_info);
        rpmi_add_common_test(prefix, "base/probe-service-groups", machine,
                             test_rpmi_common_base_probe_service_groups);
    }

    if (coverage & RPMI_QTEST_SYSRESET) {
        rpmi_add_common_test(prefix, "sysreset/attrs", machine,
                             test_rpmi_common_sysreset_attrs);
        rpmi_add_common_test(prefix, "sysreset/shutdown", machine,
                             test_rpmi_common_sysreset_shutdown);
        rpmi_add_common_test(prefix, "sysreset/cold-reboot", machine,
                             test_rpmi_common_sysreset_cold_reboot);
        rpmi_add_common_test(prefix, "sysreset/invalid-type", machine,
                             test_rpmi_common_sysreset_invalid_type);
    }

    if (coverage & RPMI_QTEST_RESET) {
        rpmi_add_common_test(prefix, "reset/clears-transport", machine,
                             test_rpmi_common_reset_clears_transport);
    }

    if (coverage & RPMI_QTEST_NEGATIVE) {
        rpmi_add_common_test(prefix, "negative/doorbell-invalid-access",
                             machine,
                             test_rpmi_common_doorbell_invalid_access);
        rpmi_add_common_test(prefix, "negative/queue-bounds", machine,
                             test_rpmi_common_queue_bounds);
    }

    if (coverage & RPMI_QTEST_REPEATED_RESET) {
        rpmi_add_common_test(prefix, "reset/repeated-after-traffic", machine,
                             test_rpmi_common_repeated_reset_after_traffic);
    }

    if (coverage & RPMI_QTEST_MIGRATION) {
        rpmi_add_common_test(prefix, "migration/blocked", machine,
                             test_rpmi_common_migration_blocked);
    }

    if (coverage & RPMI_QTEST_HSM) {
        rpmi_add_common_test(prefix, "hsm/hart-list", machine,
                             test_rpmi_common_hsm_hart_list);
        rpmi_add_common_test(prefix, "hsm/hart-status", machine,
                             test_rpmi_common_hsm_hart_status);
        rpmi_add_common_test(prefix, "hsm/hart-control", machine,
                             test_rpmi_common_hsm_hart_control);
    }

    if (coverage & RPMI_QTEST_HSM_MULTI_SOCKET) {
        rpmi_add_common_test(prefix, "hsm/multi-socket-hart-list", machine,
                             test_rpmi_common_hsm_multi_socket_hart_list);
    }

    if (coverage & RPMI_QTEST_SYSSUSP) {
        rpmi_add_common_test(prefix, "syssusp/attrs-and-suspend", machine,
                             test_rpmi_common_syssusp_attrs_and_suspend);
    }
}
