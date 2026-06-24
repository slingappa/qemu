/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * QTests for RISC-V RPMI devices.
 */

#include "qemu/osdep.h"
#include <glib/gstdio.h>
#include "libqtest.h"
#include "qobject/qdict.h"

#define RPMI_SHMEM_BASE 0x10200000ULL
#define RPMI_DOORBELL_BASE 0x10230000ULL
#define RPMI_IMSIC_S_BASE 0x28000000ULL
#define RPMI_CPPC_FASTCHAN_BASE 0x10240000ULL
#define RPMI_CPPC_FASTCHAN_SIZE 0x4000
#define RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET 0x2000
#define RPMI_SLOT_SIZE 64

#define RPMI_A2P_HEAD RPMI_SHMEM_BASE
#define RPMI_A2P_TAIL (RPMI_SHMEM_BASE + RPMI_SLOT_SIZE)
#define RPMI_A2P_SLOT0 (RPMI_SHMEM_BASE + 2 * RPMI_SLOT_SIZE)

#define RPMI_SRVGRP_BASE 0x0001
#define RPMI_SRVGRP_SYSTEM_MSI 0x0002
#define RPMI_SRVGRP_SYSTEM_RESET 0x0003
#define RPMI_SRVGRP_SYSTEM_SUSPEND 0x0004
#define RPMI_SRVGRP_HSM 0x0005
#define RPMI_SRVGRP_CPPC 0x0006
#define RPMI_SRVGRP_CLOCK 0x0008
#define RPMI_SRVGRP_MANAGEMENT_MODE 0x000b
#define RPMI_SRVGRP_LOGGING 0x000e
#define RPMI_BASE_SRV_GET_PLATFORM_INFO 0x05
#define RPMI_BASE_SRV_PROBE_SERVICE_GROUP 0x06
#define RPMI_SYSMSI_SRV_GET_ATTRIBUTES 0x02
#define RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES 0x03
#define RPMI_SYSMSI_SRV_SET_MSI_STATE 0x04
#define RPMI_SYSMSI_SRV_GET_MSI_STATE 0x05
#define RPMI_SYSMSI_SRV_SET_MSI_TARGET 0x06
#define RPMI_SYSMSI_SRV_GET_MSI_TARGET 0x07
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
#define RPMI_CPPC_SRV_PROBE_REG 0x02
#define RPMI_CPPC_SRV_READ_REG 0x03
#define RPMI_CPPC_SRV_WRITE_REG 0x04
#define RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION 0x05
#define RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET 0x06
#define RPMI_CPPC_SRV_GET_HART_LIST 0x07
#define RPMI_CPPC_NOMINAL_PERF 0x01
#define RPMI_CPPC_DESIRED_PERF 0x05
#define RPMI_CPPC_COUNTER_WRAPAROUND_TIME 0x0a
#define RPMI_CPPC_REFERENCE_PERF_COUNTER 0x0b
#define RPMI_CPPC_DELIVERED_PERF_COUNTER 0x0c
#define RPMI_CPPC_REG_LEN_64 64
#define RPMI_CLK_SRV_GET_NUM_CLOCKS 0x02
#define RPMI_CLK_SRV_GET_ATTRIBUTES 0x03
#define RPMI_CLK_SRV_GET_SUPPORTED_RATES 0x04
#define RPMI_CLK_SRV_SET_CONFIG 0x05
#define RPMI_CLK_SRV_GET_CONFIG 0x06
#define RPMI_CLK_SRV_SET_RATE 0x07
#define RPMI_CLK_SRV_GET_RATE 0x08
#define RPMI_MM_SRV_GET_ATTRIBUTES 0x02
#define RPMI_MM_SRV_COMMUNICATE 0x03
#define RPMI_LOGGING_SRV_SET_CONFIG 0x02
#define RPMI_MSG_NORMAL_REQUEST 0x00
#define RPMI_MSG_POSTED_REQUEST 0x01
#define RPMI_MSG_ACKNOWLEDGEMENT 0x02
#define RPMI_SYSRST_TYPE_SHUTDOWN 0x00
#define RPMI_SYSRST_TYPE_COLD_REBOOT 0x01
#define RPMI_SYSRST_TYPE_INVALID 0x03
#define RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE 1
#define RPMI_SYS_MSI_SHUTDOWN_INDEX 0
#define RPMI_SYS_MSI_REBOOT_INDEX 1
#define RPMI_SYS_MSI_SUSPEND_INDEX 2
#define RPMI_SYS_NUM_MSI 4
#define RPMI_SYSMSI_MSI_STATE_ENABLE 1
#define RPMI_SYSMSI_MSI_STATE_PENDING 2
#define RPMI_TOKEN 0x55aa
#define RPMI_ERR_NOTSUPP 0xfffffffeU
#define RPMI_ERR_INVALID_PARAM 0xfffffffdU
#define RPMI_ERR_INVALID_ADDR 0xfffffffbU
#define RPMI_ERR_DENIED 0xfffffffcU
#define RPMI_HSM_HART_STATE_STARTED 0x00
#define RPMI_HSM_HART_STATE_STOPPED 0x01
#define RPMI_HSM_HART_STATE_SUSPENDED 0x04
#define VIRT_RPMI_CPPC_NOMINAL_PERF 30
#define VIRT_RPMI_CLOCK_COUNT 6
#define VIRT_RPMI_MM_VERSION 0x10000
#define VIRT_RPMI_SHMEM_SIZE 0x20000
#define RPMI_MM_INPUT_OFFSET 0x3000
#define RPMI_MM_OUTPUT_OFFSET 0x3800
#define RPMI_MM_BUFFER_SIZE 0x400
#define RPMI_MM_INPUT_BASE (RPMI_SHMEM_BASE + RPMI_MM_INPUT_OFFSET)
#define RPMI_MM_OUTPUT_BASE (RPMI_SHMEM_BASE + RPMI_MM_OUTPUT_OFFSET)
#define MM_EFI_COMM_HEADER_SIZE 24
#define EFI_VAR_COMM_HEADER_SIZE 16
#define EFI_VAR_ACCESS_NAME_OFFSET 36
#define EFI_VAR_NEXT_NAME_OFFSET 24
#define EFI_VAR_FN_GET_VARIABLE 1
#define EFI_VAR_FN_GET_NEXT_VARIABLE_NAME 2
#define EFI_VAR_FN_SET_VARIABLE 3
#define EFI_SUCCESS 0ULL
#define EFI_INVALID_PARAMETER 0x8000000000000002ULL
#define EFI_BUFFER_TOO_SMALL 0x8000000000000005ULL
#define EFI_NOT_FOUND 0x800000000000000eULL
#define EFI_VARIABLE_NON_VOLATILE 0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS 0x00000004

#define RPMI_P2A_ACK_BASE (RPMI_SHMEM_BASE + 16 * RPMI_SLOT_SIZE)
#define RPMI_P2A_ACK_HEAD RPMI_P2A_ACK_BASE
#define RPMI_P2A_ACK_TAIL (RPMI_P2A_ACK_BASE + RPMI_SLOT_SIZE)
#define RPMI_P2A_ACK_SLOT0 (RPMI_P2A_ACK_BASE + 2 * RPMI_SLOT_SIZE)

static uint64_t rpmi_response_base;

static uint64_t rpmi_queue_slot(uint64_t queue_base, uint32_t index)
{
    return queue_base + (index + 2) * RPMI_SLOT_SIZE;
}

static void rpmi_send_request(QTestState *qts, uint16_t service_group,
                              uint8_t service_id, uint8_t request_type,
                              const uint32_t *data, size_t data_words)
{
    uint32_t tail = qtest_readl(qts, RPMI_A2P_TAIL);
    uint64_t slot = rpmi_queue_slot(RPMI_SHMEM_BASE, tail);
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
        (uint64_t)RPMI_SHMEM_BASE, (uint64_t)RPMI_DOORBELL_BASE,
        service_group, service_id, request_type, data_words * sizeof(*data),
        RPMI_TOKEN, tail, slot);

    qtest_writel(qts, RPMI_A2P_TAIL, (tail + 1) % 16);
    qtest_writel(qts, RPMI_DOORBELL_BASE, 1);
}

static uint32_t rpmi_response_word(QTestState *qts, unsigned int word)
{
    return qtest_readl(qts, rpmi_response_base + 8 + word * sizeof(uint32_t));
}

static void rpmi_expect_ack(QTestState *qts, uint16_t service_group,
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
        (uint64_t)RPMI_SHMEM_BASE, service_group, service_id,
        RPMI_MSG_ACKNOWLEDGEMENT, data_len, RPMI_TOKEN, head,
        rpmi_response_base,
        data_len >= sizeof(uint32_t) ? rpmi_response_word(qts, 0) : 0);
    qtest_writel(qts, RPMI_P2A_ACK_HEAD, (head + 1) % 16);
}

static void rpmi_send_sysreset(QTestState *qts, uint32_t reset_type,
                               uint8_t request_type)
{
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_RESET,
                      RPMI_SYSRST_SRV_SYSTEM_RESET, request_type,
                      &reset_type, 1);
}

static void rpmi_expect_qemu_failure(const char *extra_args,
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

static void test_rpmi_machine_realize_off(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=off");
    qtest_quit(qts);
}

static void test_rpmi_machine_rejects_too_many_harts(void)
{
    rpmi_expect_qemu_failure(
        "-machine virt,rpmi=on -smp 513 -display none -S",
        "max CPUs supported by machine 'virt' is 512");
}

static void test_rpmi_base_platform_info(void)
{
    static const char expected[] = "QEMU RISC-V virt RPMI";
    QTestState *qts;
    size_t i;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_BASE,
                      RPMI_BASE_SRV_GET_PLATFORM_INFO,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);

    rpmi_expect_ack(qts, RPMI_SRVGRP_BASE,
                    RPMI_BASE_SRV_GET_PLATFORM_INFO,
                    2 * sizeof(uint32_t) + sizeof(expected));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, sizeof(expected));
    for (i = 0; i < sizeof(expected); i++) {
        g_assert_cmphex(qtest_readb(qts, RPMI_P2A_ACK_SLOT0 + 16 + i), ==,
                        expected[i]);
    }

    qtest_quit(qts);
}

static void rpmi_probe_group(QTestState *qts, uint32_t service_group,
                             bool present)
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

static void test_rpmi_base_probe_service_groups(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_probe_group(qts, RPMI_SRVGRP_BASE, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_RESET, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_HSM, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_SUSPEND, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_CPPC, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_MSI, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_CLOCK, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_MANAGEMENT_MODE, true);
    qtest_system_reset(qts);
#ifdef CONFIG_LIBRPMI_LOGGING
    rpmi_probe_group(qts, RPMI_SRVGRP_LOGGING, true);
#else
    rpmi_probe_group(qts, RPMI_SRVGRP_LOGGING, false);
#endif

    qtest_quit(qts);
}

static void test_rpmi_base_probe_sysmsi_without_imsic(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on,aia=none");
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_RESET, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_HSM, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_SUSPEND, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_CPPC, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_CLOCK, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_MANAGEMENT_MODE, true);
    qtest_system_reset(qts);
    rpmi_probe_group(qts, RPMI_SRVGRP_SYSTEM_MSI, false);

    qtest_quit(qts);
}

static void test_rpmi_sysreset_attrs(void)
{
    QTestState *qts;
    uint32_t reset_type = RPMI_SYSRST_TYPE_SHUTDOWN;

    qts = qtest_init("-machine virt,rpmi=on");
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

static void test_rpmi_sysreset_shutdown(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_SHUTDOWN,
                       RPMI_MSG_POSTED_REQUEST);
    qtest_qmp_eventwait(qts, "SHUTDOWN");
    qtest_quit(qts);
}

static void test_rpmi_sysreset_cold_reboot(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on -no-reboot");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_COLD_REBOOT,
                       RPMI_MSG_POSTED_REQUEST);
    qtest_qmp_eventwait(qts, "SHUTDOWN");
    qtest_quit(qts);
}

static void test_rpmi_sysreset_invalid_type(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_INVALID,
                       RPMI_MSG_NORMAL_REQUEST);

    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_SYSTEM_RESET, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_PARAM);

    qtest_quit(qts);
}

static void test_rpmi_repeated_reset_after_traffic(void)
{
    QTestState *qts;
    uint32_t reset_type = RPMI_SYSRST_TYPE_SHUTDOWN;

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
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

static void test_rpmi_reset_clears_transport(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_sysreset(qts, RPMI_SYSRST_TYPE_INVALID,
                       RPMI_MSG_NORMAL_REQUEST);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_RESET,
                    RPMI_SYSRST_SRV_SYSTEM_RESET, sizeof(uint32_t));

    qtest_system_reset(qts);

    g_assert_cmphex(qtest_readl(qts, RPMI_A2P_TAIL), ==, 0);
    g_assert_cmphex(qtest_readl(qts, RPMI_P2A_ACK_TAIL), ==, 0);
    g_assert_cmphex(qtest_readl(qts, RPMI_DOORBELL_BASE), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_doorbell_invalid_access(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    qtest_writeb(qts, RPMI_DOORBELL_BASE, 1);
    qtest_writel(qts, RPMI_DOORBELL_BASE + 4, 1);
    g_assert_cmphex(qtest_readl(qts, RPMI_DOORBELL_BASE), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_queue_bounds(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    qtest_writel(qts, RPMI_A2P_TAIL, 0x1000);
    qtest_writel(qts, RPMI_DOORBELL_BASE, 1);
    g_assert_cmphex(qtest_readl(qts, RPMI_A2P_HEAD), ==, 0);
    g_assert_cmphex(qtest_readl(qts, RPMI_P2A_ACK_TAIL), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_migration_blocked(void)
{
    QTestState *qts;
    QDict *error;
    const char *desc;

    qts = qtest_init("-machine virt,rpmi=on -S");
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

static void test_rpmi_hsm_hart_list(void)
{
    QTestState *qts;
    uint32_t start_index = 0;

    qts = qtest_init("-machine virt,rpmi=on -smp 4");
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                      RPMI_MSG_NORMAL_REQUEST, &start_index, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                    7 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 4);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, 1);
    g_assert_cmphex(rpmi_response_word(qts, 5), ==, 2);
    g_assert_cmphex(rpmi_response_word(qts, 6), ==, 3);

    qtest_quit(qts);
}

static void test_rpmi_hsm_multi_socket_hart_list(void)
{
    QTestState *qts;
    uint32_t start_index = 0;

    qts = qtest_init("-machine virt,rpmi=on "
                     "-smp 4,sockets=2,cores=2,threads=1");
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                      RPMI_MSG_NORMAL_REQUEST, &start_index, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_LIST,
                    7 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 4);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, 1);
    g_assert_cmphex(rpmi_response_word(qts, 5), ==, 2);
    g_assert_cmphex(rpmi_response_word(qts, 6), ==, 3);

    qtest_quit(qts);
}

static void test_rpmi_hsm_hart_status(void)
{
    QTestState *qts;
    uint32_t hart_id = 3;

    qts = qtest_init("-machine virt,rpmi=on -smp 4");
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                      RPMI_MSG_NORMAL_REQUEST, &hart_id, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_HSM_HART_STATE_STARTED);

    qtest_quit(qts);
}

static void rpmi_expect_hsm_status(QTestState *qts, uint32_t hart_id,
                                   uint32_t expected_state)
{
    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                      RPMI_MSG_NORMAL_REQUEST, &hart_id, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_GET_HART_STATUS,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, expected_state);
}

static void test_rpmi_hsm_hart_control(void)
{
    QTestState *qts;
    uint32_t hart_id = 1;
    uint32_t stop_request[] = { hart_id };
    uint32_t start_request[] = { hart_id, 0x80000000, 0 };
    uint32_t suspend_request[] = { hart_id, 0, 0x80001000, 0 };
    uint32_t start_index = 0;
    uint32_t suspend_type = 0;

    qts = qtest_init("-machine virt,rpmi=on -smp 2");
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

    rpmi_send_request(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_SUSPEND,
                      RPMI_MSG_NORMAL_REQUEST, suspend_request,
                      ARRAY_SIZE(suspend_request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_HSM, RPMI_HSM_SRV_HART_SUSPEND,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    rpmi_expect_hsm_status(qts, hart_id, RPMI_HSM_HART_STATE_SUSPENDED);

    qtest_quit(qts);
}

static void test_rpmi_syssusp_attrs_and_suspend(void)
{
    QTestState *qts;
    uint32_t suspend_type = 0;
    uint32_t suspend_request[] = { 0, 0, 0x80000000, 0 };

    qts = qtest_init("-machine virt,rpmi=on -smp 1");
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

static void test_rpmi_cppc_hart_list(void)
{
    QTestState *qts;
    uint32_t start_index = 0;

    qts = qtest_init("-machine virt,rpmi=on -smp 4");
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_GET_HART_LIST,
                      RPMI_MSG_NORMAL_REQUEST, &start_index, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_GET_HART_LIST,
                    7 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 4);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, 1);
    g_assert_cmphex(rpmi_response_word(qts, 5), ==, 2);
    g_assert_cmphex(rpmi_response_word(qts, 6), ==, 3);

    qtest_quit(qts);
}

static void test_rpmi_cppc_read_nominal_perf(void)
{
    QTestState *qts;
    uint32_t request[] = { 0, RPMI_CPPC_NOMINAL_PERF };

    qts = qtest_init("-machine virt,rpmi=on");
    g_assert_cmphex(qtest_readl(qts, RPMI_CPPC_FASTCHAN_BASE), ==,
                    VIRT_RPMI_CPPC_NOMINAL_PERF);

    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, request, ARRAY_SIZE(request));

    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    VIRT_RPMI_CPPC_NOMINAL_PERF);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_cppc_invalid_hart(void)
{
    QTestState *qts;
    uint32_t request[] = { 0xff, RPMI_CPPC_NOMINAL_PERF };

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, request, ARRAY_SIZE(request));

    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_PARAM);

    qtest_quit(qts);
}

static void test_rpmi_cppc_fast_channel_region(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC,
                      RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);

    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC,
                    RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION,
                    12 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==,
                    RPMI_CPPC_FASTCHAN_BASE);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==,
                    RPMI_CPPC_FASTCHAN_SIZE);
    g_assert_cmphex(rpmi_response_word(qts, 5), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_cppc_fast_channel_offset(void)
{
    QTestState *qts;
    uint32_t hart_id = 1;

    qts = qtest_init("-machine virt,rpmi=on -smp 2");
    g_assert_cmphex(qtest_readl(qts, RPMI_CPPC_FASTCHAN_BASE + 8), ==,
                    VIRT_RPMI_CPPC_NOMINAL_PERF);

    rpmi_send_request(qts, RPMI_SRVGRP_CPPC,
                      RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET,
                      RPMI_MSG_NORMAL_REQUEST, &hart_id, 1);

    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC,
                    RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET,
                    5 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 8);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==,
                    RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET + 8);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, 0);

    qtest_quit(qts);
}

static void test_rpmi_cppc_counters_and_depth(void)
{
    QTestState *qts;
    uint32_t probe_reference[] = { 0, RPMI_CPPC_REFERENCE_PERF_COUNTER };
    uint32_t probe_delivered[] = { 0, RPMI_CPPC_DELIVERED_PERF_COUNTER };
    uint32_t probe_wrap[] = { 0, RPMI_CPPC_COUNTER_WRAPAROUND_TIME };
    uint32_t read_reference[] = { 0, RPMI_CPPC_REFERENCE_PERF_COUNTER };
    uint32_t read_delivered[] = { 0, RPMI_CPPC_DELIVERED_PERF_COUNTER };
    uint64_t reference_before;
    uint64_t reference_after;
    uint64_t delivered_before;
    uint64_t delivered_after;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                      RPMI_MSG_NORMAL_REQUEST, probe_reference,
                      ARRAY_SIZE(probe_reference));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, RPMI_CPPC_REG_LEN_64);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                      RPMI_MSG_NORMAL_REQUEST, probe_delivered,
                      ARRAY_SIZE(probe_delivered));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, RPMI_CPPC_REG_LEN_64);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                      RPMI_MSG_NORMAL_REQUEST, probe_wrap,
                      ARRAY_SIZE(probe_wrap));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_PROBE_REG,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, RPMI_ERR_NOTSUPP);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, read_reference,
                      ARRAY_SIZE(read_reference));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    reference_before = rpmi_response_word(qts, 1) |
                       ((uint64_t)rpmi_response_word(qts, 2) << 32);

    qtest_clock_step(qts, 1000000000LL);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, read_reference,
                      ARRAY_SIZE(read_reference));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    reference_after = rpmi_response_word(qts, 1) |
                      ((uint64_t)rpmi_response_word(qts, 2) << 32);
    g_assert_cmpuint(reference_after, >, reference_before);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, read_delivered,
                      ARRAY_SIZE(read_delivered));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    delivered_before = rpmi_response_word(qts, 1) |
                       ((uint64_t)rpmi_response_word(qts, 2) << 32);

    qtest_clock_step(qts, 1000000000LL);
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                      RPMI_MSG_NORMAL_REQUEST, read_delivered,
                      ARRAY_SIZE(read_delivered));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_READ_REG,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    delivered_after = rpmi_response_word(qts, 1) |
                      ((uint64_t)rpmi_response_word(qts, 2) << 32);
    g_assert_cmpuint(delivered_after, >, delivered_before);

    qtest_quit(qts);
}

static void test_rpmi_cppc_fast_channel_perf_update(void)
{
    QTestState *qts;
    uint64_t nominal_freq = 3000000000ULL;
    uint64_t highest_freq = 3200000000ULL;

    qts = qtest_init("-machine virt,rpmi=on");
    qtest_writel(qts, RPMI_CPPC_FASTCHAN_BASE, 33);
    qtest_writel(qts, RPMI_DOORBELL_BASE, 1);
    g_assert_cmphex(qtest_readq(qts, RPMI_CPPC_FASTCHAN_BASE +
                                RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET), ==,
                    nominal_freq);
    g_test_message(
        "RPMI_CPPC_FASTCHAN base=0x%016" PRIx64
        " request_offset=0x0 value=%u feedback_offset=0x%x feedback=%" PRIu64,
        (uint64_t)RPMI_CPPC_FASTCHAN_BASE, 33,
        RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET, nominal_freq);

    qtest_writel(qts, RPMI_CPPC_FASTCHAN_BASE, 32);
    qtest_writel(qts, RPMI_DOORBELL_BASE, 1);
    g_assert_cmphex(qtest_readq(qts, RPMI_CPPC_FASTCHAN_BASE +
                                RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET), ==,
                    highest_freq);
    g_test_message(
        "RPMI_CPPC_FASTCHAN base=0x%016" PRIx64
        " request_offset=0x0 value=%u feedback_offset=0x%x feedback=%" PRIu64,
        (uint64_t)RPMI_CPPC_FASTCHAN_BASE, 32,
        RPMI_CPPC_FASTCHAN_FEEDBACK_OFFSET, highest_freq);

    qtest_quit(qts);
}

static void test_rpmi_cppc_write_reg_denied(void)
{
    QTestState *qts;
    uint32_t write_desired_perf[] = { 0, RPMI_CPPC_DESIRED_PERF, 31, 0 };

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_WRITE_REG,
                      RPMI_MSG_NORMAL_REQUEST, write_desired_perf,
                      ARRAY_SIZE(write_desired_perf));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CPPC, RPMI_CPPC_SRV_WRITE_REG,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, RPMI_ERR_DENIED);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_attrs(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);

    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_GET_ATTRIBUTES, 4 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_SYS_NUM_MSI);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_rejects_bad_addr(void)
{
    QTestState *qts;
    uint32_t request[] = { RPMI_SYS_MSI_SHUTDOWN_INDEX, 0, 0, 0x1234 };

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_SET_MSI_TARGET,
                      RPMI_MSG_NORMAL_REQUEST, request, ARRAY_SIZE(request));

    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_SET_MSI_TARGET, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_ADDR);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_msi_attrs_and_state(void)
{
    QTestState *qts;
    uint32_t msi_index = RPMI_SYS_MSI_SHUTDOWN_INDEX;
    uint32_t set_state[] = {
        RPMI_SYS_MSI_SHUTDOWN_INDEX, RPMI_SYSMSI_MSI_STATE_ENABLE
    };

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &msi_index, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
                    7 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);
    char name[9];
    for (size_t i = 0; i < sizeof(name) - 1; i++) {
        name[i] = qtest_readb(qts, RPMI_P2A_ACK_SLOT0 + 20 + i);
    }
    name[sizeof(name) - 1] = '\0';
    g_assert_cmpstr(name, ==, "shutdown");

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_SET_MSI_STATE,
                      RPMI_MSG_NORMAL_REQUEST, set_state,
                      ARRAY_SIZE(set_state));
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_SET_MSI_STATE, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_GET_MSI_STATE,
                      RPMI_MSG_NORMAL_REQUEST, &msi_index, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_GET_MSI_STATE, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_SYSMSI_MSI_STATE_ENABLE);

    qtest_quit(qts);
}

static void rpmi_sysmsi_set_state(QTestState *qts, uint32_t msi_index,
                                  uint32_t state)
{
    uint32_t set_state[] = { msi_index, state };

    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_SET_MSI_STATE,
                      RPMI_MSG_NORMAL_REQUEST, set_state,
                      ARRAY_SIZE(set_state));
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_SET_MSI_STATE, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
}

static void rpmi_sysmsi_expect_state(QTestState *qts, uint32_t msi_index,
                                     uint32_t state)
{
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_GET_MSI_STATE,
                      RPMI_MSG_NORMAL_REQUEST, &msi_index, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_GET_MSI_STATE, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, state);
}

static void test_rpmi_sysmsi_powerdown_injects_pending(void)
{
    QTestState *qts;
    uint32_t msi_index = RPMI_SYS_MSI_SHUTDOWN_INDEX;

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_sysmsi_set_state(qts, msi_index, RPMI_SYSMSI_MSI_STATE_ENABLE);

    qtest_qmp_assert_success(qts, "{ 'execute': 'system_powerdown' }");
    qtest_qmp_eventwait(qts, "POWERDOWN");

    rpmi_sysmsi_expect_state(qts, msi_index,
                             RPMI_SYSMSI_MSI_STATE_ENABLE |
                             RPMI_SYSMSI_MSI_STATE_PENDING);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_reset_injects_pending(void)
{
    QTestState *qts;
    uint32_t msi_index = RPMI_SYS_MSI_REBOOT_INDEX;

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_sysmsi_set_state(qts, msi_index, RPMI_SYSMSI_MSI_STATE_ENABLE);
    qtest_system_reset(qts);
    rpmi_sysmsi_expect_state(qts, msi_index,
                             RPMI_SYSMSI_MSI_STATE_ENABLE |
                             RPMI_SYSMSI_MSI_STATE_PENDING);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_suspend_injects_pending(void)
{
    QTestState *qts;
    uint32_t msi_index = RPMI_SYS_MSI_SUSPEND_INDEX;
    uint32_t suspend_request[] = { 0, 0, 0x80000000, 0 };

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic -smp 1");
    rpmi_sysmsi_set_state(qts, msi_index, RPMI_SYSMSI_MSI_STATE_ENABLE);
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
    rpmi_sysmsi_expect_state(qts, msi_index,
                             RPMI_SYSMSI_MSI_STATE_ENABLE |
                             RPMI_SYSMSI_MSI_STATE_PENDING);

    qtest_quit(qts);
}

static void test_rpmi_sysmsi_target_roundtrip(void)
{
    QTestState *qts;
    uint32_t set_target[] = {
        RPMI_SYS_MSI_SHUTDOWN_INDEX, RPMI_IMSIC_S_BASE, 0, 0x1234
    };
    uint32_t get_target[] = { RPMI_SYS_MSI_SHUTDOWN_INDEX };

    qts = qtest_init("-machine virt,rpmi=on,aia=aplic-imsic");
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_SET_MSI_TARGET,
                      RPMI_MSG_NORMAL_REQUEST, set_target,
                      ARRAY_SIZE(set_target));
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_SET_MSI_TARGET, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_SYSTEM_MSI,
                      RPMI_SYSMSI_SRV_GET_MSI_TARGET,
                      RPMI_MSG_NORMAL_REQUEST, get_target,
                      ARRAY_SIZE(get_target));
    rpmi_expect_ack(qts, RPMI_SRVGRP_SYSTEM_MSI,
                    RPMI_SYSMSI_SRV_GET_MSI_TARGET, 4 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==,
                    RPMI_IMSIC_S_BASE);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0x1234);

    qtest_quit(qts);
}

static void test_rpmi_clock_rates(void)
{
    QTestState *qts;
    uint32_t rate_query[] = { 0, 0 };
    uint32_t clock_id = 0;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK,
                      RPMI_CLK_SRV_GET_SUPPORTED_RATES,
                      RPMI_MSG_NORMAL_REQUEST, rate_query,
                      ARRAY_SIZE(rate_query));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK,
                    RPMI_CLK_SRV_GET_SUPPORTED_RATES,
                    10 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 3);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, 0x22222222);
    g_assert_cmphex(rpmi_response_word(qts, 5), ==, 0x11111111);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_RATE,
                      RPMI_MSG_NORMAL_REQUEST, &clock_id, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_RATE,
                    3 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0x22222222);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 0x11111111);

    qtest_quit(qts);
}

static void test_rpmi_clock_commands(void)
{
    QTestState *qts;
    uint32_t clock_id = 0;
    uint32_t clock_config[] = { 2, 0 };

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_NUM_CLOCKS,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_NUM_CLOCKS,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, VIRT_RPMI_CLOCK_COUNT);

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, &clock_id, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_ATTRIBUTES,
                    8 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 1);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, 3);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 100);
    char name[7];
    for (size_t i = 0; i < sizeof(name) - 1; i++) {
        name[i] = qtest_readb(qts, RPMI_P2A_ACK_SLOT0 + 24 + i);
    }
    name[sizeof(name) - 1] = '\0';
    g_assert_cmpstr(name, ==, "clock0");

    qtest_system_reset(qts);
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_CONFIG,
                      RPMI_MSG_NORMAL_REQUEST, clock_config,
                      ARRAY_SIZE(clock_config));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_CONFIG,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);

    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_CONFIG,
                      RPMI_MSG_NORMAL_REQUEST, clock_config, 1);
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_GET_CONFIG,
                    2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, 0);

    uint32_t bad_rate[] = { 0, 0, 0, 0 };
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_RATE,
                      RPMI_MSG_NORMAL_REQUEST, bad_rate,
                      ARRAY_SIZE(bad_rate));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_RATE,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_PARAM);

    uint32_t invalid_clock_config[] = { VIRT_RPMI_CLOCK_COUNT, 1 };
    rpmi_send_request(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_CONFIG,
                      RPMI_MSG_NORMAL_REQUEST, invalid_clock_config,
                      ARRAY_SIZE(invalid_clock_config));
    rpmi_expect_ack(qts, RPMI_SRVGRP_CLOCK, RPMI_CLK_SRV_SET_CONFIG,
                    sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==,
                    RPMI_ERR_INVALID_PARAM);

    qtest_quit(qts);
}

typedef struct RpmiTestGuid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} RpmiTestGuid;

static const RpmiTestGuid rpmi_mm_var_protocol_guid = {
    0xed32d533, 0x99e6, 0x4209,
    { 0x9c, 0xc0, 0x2d, 0x72, 0xcd, 0xd9, 0x98, 0xa7 }
};

static const RpmiTestGuid rpmi_mm_test_vendor_guid = {
    0x12345678, 0x9abc, 0xdef0,
    { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 }
};

static void rpmi_mm_write_guid(QTestState *qts, uint64_t addr,
                                const RpmiTestGuid *guid)
{
    qtest_writel(qts, addr, guid->data1);
    qtest_writew(qts, addr + 4, guid->data2);
    qtest_writew(qts, addr + 6, guid->data3);
    for (size_t i = 0; i < sizeof(guid->data4); i++) {
        qtest_writeb(qts, addr + 8 + i, guid->data4[i]);
    }
}

static void rpmi_mm_expect_guid(QTestState *qts, uint64_t addr,
                                const RpmiTestGuid *guid)
{
    g_assert_cmphex(qtest_readl(qts, addr), ==, guid->data1);
    g_assert_cmphex(qtest_readw(qts, addr + 4), ==, guid->data2);
    g_assert_cmphex(qtest_readw(qts, addr + 6), ==, guid->data3);
    for (size_t i = 0; i < sizeof(guid->data4); i++) {
        g_assert_cmphex(qtest_readb(qts, addr + 8 + i), ==,
                        guid->data4[i]);
    }
}

static uint64_t rpmi_mm_write_name(QTestState *qts, uint64_t addr,
                                   const char *name)
{
    size_t len = strlen(name);

    for (size_t i = 0; i < len; i++) {
        qtest_writew(qts, addr + i * sizeof(uint16_t), name[i]);
    }
    qtest_writew(qts, addr + len * sizeof(uint16_t), 0);

    return (len + 1) * sizeof(uint16_t);
}

static void rpmi_mm_expect_name(QTestState *qts, uint64_t addr,
                                const char *name)
{
    size_t len = strlen(name);

    for (size_t i = 0; i < len; i++) {
        g_assert_cmphex(qtest_readw(qts, addr + i * sizeof(uint16_t)), ==,
                        name[i]);
    }
    g_assert_cmphex(qtest_readw(qts, addr + len * sizeof(uint16_t)), ==, 0);
}

static uint64_t rpmi_mm_write_access(QTestState *qts, uint64_t function,
                                     const char *name, uint64_t datasize,
                                     uint32_t attr, const uint8_t *value)
{
    uint64_t payload_base = RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE +
                            EFI_VAR_COMM_HEADER_SIZE;
    uint64_t name_base = payload_base + EFI_VAR_ACCESS_NAME_OFFSET;
    uint64_t namesize;
    uint64_t msg_len;

    qtest_memset(qts, RPMI_MM_INPUT_BASE, 0, RPMI_MM_BUFFER_SIZE);
    qtest_memset(qts, RPMI_MM_OUTPUT_BASE, 0, RPMI_MM_BUFFER_SIZE);

    rpmi_mm_write_guid(qts, RPMI_MM_INPUT_BASE, &rpmi_mm_var_protocol_guid);
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + 16, 0);
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE,
                 function);
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE + 8, 0);
    rpmi_mm_write_guid(qts, payload_base, &rpmi_mm_test_vendor_guid);
    qtest_writeq(qts, payload_base + 16, datasize);
    namesize = rpmi_mm_write_name(qts, name_base, name);
    qtest_writeq(qts, payload_base + 24, namesize);
    qtest_writel(qts, payload_base + 32, attr);
    if (value) {
        qtest_memwrite(qts, name_base + namesize, value, datasize);
    }

    msg_len = EFI_VAR_COMM_HEADER_SIZE + EFI_VAR_ACCESS_NAME_OFFSET +
              namesize + datasize;
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + 16, msg_len);

    return MM_EFI_COMM_HEADER_SIZE + msg_len;
}

static uint64_t rpmi_mm_write_get_next(QTestState *qts, const char *name,
                                       uint64_t namesize)
{
    uint64_t payload_base = RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE +
                            EFI_VAR_COMM_HEADER_SIZE;
    uint64_t name_base = payload_base + EFI_VAR_NEXT_NAME_OFFSET;
    uint64_t msg_len;

    qtest_memset(qts, RPMI_MM_INPUT_BASE, 0, RPMI_MM_BUFFER_SIZE);
    qtest_memset(qts, RPMI_MM_OUTPUT_BASE, 0, RPMI_MM_BUFFER_SIZE);

    rpmi_mm_write_guid(qts, RPMI_MM_INPUT_BASE, &rpmi_mm_var_protocol_guid);
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE,
                 EFI_VAR_FN_GET_NEXT_VARIABLE_NAME);
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + MM_EFI_COMM_HEADER_SIZE + 8, 0);
    if (name[0]) {
        rpmi_mm_write_guid(qts, payload_base, &rpmi_mm_test_vendor_guid);
        rpmi_mm_write_name(qts, name_base, name);
    }
    qtest_writeq(qts, payload_base + 16, namesize);

    msg_len = EFI_VAR_COMM_HEADER_SIZE + EFI_VAR_NEXT_NAME_OFFSET + namesize;
    qtest_writeq(qts, RPMI_MM_INPUT_BASE + 16, msg_len);

    return MM_EFI_COMM_HEADER_SIZE + msg_len;
}

static void rpmi_mm_send_communicate(QTestState *qts, uint64_t input_len)
{
    uint32_t request[] = {
        RPMI_MM_INPUT_OFFSET, input_len, RPMI_MM_OUTPUT_OFFSET,
        RPMI_MM_BUFFER_SIZE,
    };

    rpmi_send_request(qts, RPMI_SRVGRP_MANAGEMENT_MODE,
                      RPMI_MM_SRV_COMMUNICATE, RPMI_MSG_NORMAL_REQUEST,
                      request, ARRAY_SIZE(request));
    rpmi_expect_ack(qts, RPMI_SRVGRP_MANAGEMENT_MODE,
                    RPMI_MM_SRV_COMMUNICATE, 2 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_test_message(
        "RPMI_MM_COMMUNICATE input=0x%016" PRIx64 " input_len=%" PRIu64
        " output=0x%016" PRIx64 " output_len=0x%x efi_status=0x%016" PRIx64,
        (uint64_t)RPMI_MM_INPUT_BASE, input_len,
        (uint64_t)RPMI_MM_OUTPUT_BASE, RPMI_MM_BUFFER_SIZE,
        qtest_readq(qts, RPMI_MM_OUTPUT_BASE + MM_EFI_COMM_HEADER_SIZE + 8));
}

static uint64_t rpmi_mm_return_status(QTestState *qts)
{
    return qtest_readq(qts, RPMI_MM_OUTPUT_BASE + MM_EFI_COMM_HEADER_SIZE + 8);
}

static void test_rpmi_mm_variable_roundtrip(void)
{
    QTestState *qts;
    static const uint8_t value[] = { 0xde, 0xad, 0xbe, 0xef };
    uint32_t attr = EFI_VARIABLE_NON_VOLATILE |
                    EFI_VARIABLE_BOOTSERVICE_ACCESS |
                    EFI_VARIABLE_RUNTIME_ACCESS;
    uint64_t input_len;
    uint64_t payload_base = RPMI_MM_OUTPUT_BASE + MM_EFI_COMM_HEADER_SIZE +
                            EFI_VAR_COMM_HEADER_SIZE;
    uint64_t name_base = payload_base + EFI_VAR_ACCESS_NAME_OFFSET;
    uint64_t namesize = (strlen("TestVar") + 1) * sizeof(uint16_t);

    qts = qtest_init("-machine virt,rpmi=on");
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_SET_VARIABLE,
                                     "TestVar", sizeof(value), attr, value);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);

    qtest_system_reset(qts);
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_GET_VARIABLE,
                                     "TestVar", 0, 0, NULL);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_BUFFER_TOO_SMALL);
    g_assert_cmphex(qtest_readq(qts, payload_base + 16), ==, sizeof(value));
    g_assert_cmphex(qtest_readl(qts, payload_base + 32), ==, attr);

    qtest_system_reset(qts);
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_GET_VARIABLE,
                                     "TestVar", sizeof(value), 0, NULL);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);
    g_assert_cmphex(qtest_readq(qts, payload_base + 16), ==, sizeof(value));
    g_assert_cmphex(qtest_readl(qts, payload_base + 32), ==, attr);
    for (size_t i = 0; i < sizeof(value); i++) {
        g_assert_cmphex(qtest_readb(qts, name_base + namesize + i), ==,
                        value[i]);
    }

    input_len = rpmi_mm_write_get_next(qts, "", 64);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);
    payload_base = RPMI_MM_OUTPUT_BASE + MM_EFI_COMM_HEADER_SIZE +
                   EFI_VAR_COMM_HEADER_SIZE;
    name_base = payload_base + EFI_VAR_NEXT_NAME_OFFSET;
    rpmi_mm_expect_guid(qts, payload_base, &rpmi_mm_test_vendor_guid);
    g_assert_cmphex(qtest_readq(qts, payload_base + 16), ==, namesize);
    rpmi_mm_expect_name(qts, name_base, "TestVar");

    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_SET_VARIABLE,
                                     "TestVar", 0, attr, NULL);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);

    qtest_system_reset(qts);
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_GET_VARIABLE,
                                     "TestVar", sizeof(value), 0, NULL);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_NOT_FOUND);

    qtest_quit(qts);
}

static void test_rpmi_mm_process_restart_persistence(void)
{
    QTestState *qts;
    g_autofree char *tmpdir = g_dir_make_tmp("qemu-rpmi-mm-XXXXXX", NULL);
    g_autofree char *store = g_build_filename(tmpdir, "vars.store", NULL);
    static const uint8_t value[] = { 0xca, 0xfe, 0xba, 0xbe };
    uint32_t attr = EFI_VARIABLE_NON_VOLATILE |
                    EFI_VARIABLE_BOOTSERVICE_ACCESS |
                    EFI_VARIABLE_RUNTIME_ACCESS;
    uint64_t input_len;
    uint64_t payload_base = RPMI_MM_OUTPUT_BASE + MM_EFI_COMM_HEADER_SIZE +
                            EFI_VAR_COMM_HEADER_SIZE;
    uint64_t name_base = payload_base + EFI_VAR_ACCESS_NAME_OFFSET;
    uint64_t namesize = (strlen("PersistVar") + 1) * sizeof(uint16_t);

    g_assert_nonnull(tmpdir);

    qts = qtest_initf("-machine virt,rpmi=on,rpmi-mm-store=%s", store);
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_SET_VARIABLE,
                                     "PersistVar", sizeof(value), attr,
                                     value);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);
    qtest_quit(qts);

    qts = qtest_initf("-machine virt,rpmi=on,rpmi-mm-store=%s", store);
    input_len = rpmi_mm_write_access(qts, EFI_VAR_FN_GET_VARIABLE,
                                     "PersistVar", sizeof(value), 0, NULL);
    rpmi_mm_send_communicate(qts, input_len);
    g_assert_cmphex(rpmi_mm_return_status(qts), ==, EFI_SUCCESS);
    g_assert_cmphex(qtest_readq(qts, payload_base + 16), ==, sizeof(value));
    g_assert_cmphex(qtest_readl(qts, payload_base + 32), ==, attr);
    for (size_t i = 0; i < sizeof(value); i++) {
        g_assert_cmphex(qtest_readb(qts, name_base + namesize + i), ==,
                        value[i]);
    }
    qtest_quit(qts);

    g_assert_cmpint(g_remove(store), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_rpmi_mm_rejects_bad_range(void)
{
    QTestState *qts;
    uint32_t request[] = {
        VIRT_RPMI_SHMEM_SIZE - 1, MM_EFI_COMM_HEADER_SIZE,
        RPMI_MM_OUTPUT_OFFSET, RPMI_MM_BUFFER_SIZE,
    };

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_MANAGEMENT_MODE,
                      RPMI_MM_SRV_COMMUNICATE, RPMI_MSG_NORMAL_REQUEST,
                      request, ARRAY_SIZE(request));
    g_assert_cmphex(qtest_readl(qts, RPMI_P2A_ACK_HEAD), ==,
                    qtest_readl(qts, RPMI_P2A_ACK_TAIL));

    qtest_quit(qts);
}


static void test_rpmi_mm_rejects_bad_store(void)
{
    g_autofree char *store = NULL;
    g_autofree char *quoted_store = NULL;
    g_autofree char *args = NULL;
    char tmpdir[] = "/tmp/rpmi-mm-store-bad-XXXXXX";

    g_assert_nonnull(g_mkdtemp(tmpdir));
    store = g_build_filename(tmpdir, "store.ini", NULL);
    g_assert_true(g_file_set_contents(store,
                                      "[rpmi-mm]\n"
                                      "version=1\n"
                                      "count=101\n",
                                      -1, NULL));

    quoted_store = g_shell_quote(store);
    args = g_strdup_printf("-machine virt,rpmi=on,rpmi-mm-store=%s "
                           "-display none -S", quoted_store);
    rpmi_expect_qemu_failure(args, "RPMI MM store has too many variables");

    g_assert_cmpint(g_remove(store), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

#ifdef CONFIG_LIBRPMI_LOGGING
static void test_rpmi_logging_set_config(void)
{
    QTestState *qts;
    uint32_t config[] = { 1, 0x11223344, 0x55667788, 0x99aabbcc };

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_LOGGING,
                      RPMI_LOGGING_SRV_SET_CONFIG,
                      RPMI_MSG_NORMAL_REQUEST, config,
                      ARRAY_SIZE(config));
    rpmi_expect_ack(qts, RPMI_SRVGRP_LOGGING,
                    RPMI_LOGGING_SRV_SET_CONFIG, sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);

    qtest_quit(qts);
}
#endif

static void test_rpmi_mm_attrs(void)
{
    QTestState *qts;

    qts = qtest_init("-machine virt,rpmi=on");
    rpmi_send_request(qts, RPMI_SRVGRP_MANAGEMENT_MODE,
                      RPMI_MM_SRV_GET_ATTRIBUTES,
                      RPMI_MSG_NORMAL_REQUEST, NULL, 0);
    rpmi_expect_ack(qts, RPMI_SRVGRP_MANAGEMENT_MODE,
                    RPMI_MM_SRV_GET_ATTRIBUTES, 5 * sizeof(uint32_t));
    g_assert_cmphex(rpmi_response_word(qts, 0), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 1), ==, VIRT_RPMI_MM_VERSION);
    g_assert_cmphex(rpmi_response_word(qts, 2), ==, RPMI_SHMEM_BASE);
    g_assert_cmphex(rpmi_response_word(qts, 3), ==, 0);
    g_assert_cmphex(rpmi_response_word(qts, 4), ==, VIRT_RPMI_SHMEM_SIZE);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    if (qtest_has_machine("virt")) {
        qtest_add_func("/riscv/rpmi/machine/realize-off",
                       test_rpmi_machine_realize_off);
        qtest_add_func("/riscv/rpmi/machine/rejects-too-many-harts",
                       test_rpmi_machine_rejects_too_many_harts);
        qtest_add_func("/riscv/rpmi/base/platform-info",
                       test_rpmi_base_platform_info);
        qtest_add_func("/riscv/rpmi/base/probe-service-groups",
                       test_rpmi_base_probe_service_groups);
        qtest_add_func("/riscv/rpmi/base/probe-sysmsi-without-imsic",
                       test_rpmi_base_probe_sysmsi_without_imsic);
        qtest_add_func("/riscv/rpmi/sysreset/attrs",
                       test_rpmi_sysreset_attrs);
        qtest_add_func("/riscv/rpmi/sysreset/shutdown",
                       test_rpmi_sysreset_shutdown);
        qtest_add_func("/riscv/rpmi/sysreset/cold-reboot",
                       test_rpmi_sysreset_cold_reboot);
        qtest_add_func("/riscv/rpmi/sysreset/invalid-type",
                       test_rpmi_sysreset_invalid_type);
        qtest_add_func("/riscv/rpmi/reset/clears-transport",
                       test_rpmi_reset_clears_transport);
        qtest_add_func("/riscv/rpmi/negative/doorbell-invalid-access",
                       test_rpmi_doorbell_invalid_access);
        qtest_add_func("/riscv/rpmi/negative/queue-bounds",
                       test_rpmi_queue_bounds);
        qtest_add_func("/riscv/rpmi/reset/repeated-after-traffic",
                       test_rpmi_repeated_reset_after_traffic);
        qtest_add_func("/riscv/rpmi/migration/blocked",
                       test_rpmi_migration_blocked);
        qtest_add_func("/riscv/rpmi/hsm/hart-list",
                       test_rpmi_hsm_hart_list);
        qtest_add_func("/riscv/rpmi/hsm/multi-socket-hart-list",
                       test_rpmi_hsm_multi_socket_hart_list);
        qtest_add_func("/riscv/rpmi/hsm/hart-status",
                       test_rpmi_hsm_hart_status);
        qtest_add_func("/riscv/rpmi/hsm/hart-control",
                       test_rpmi_hsm_hart_control);
        qtest_add_func("/riscv/rpmi/syssusp/attrs-and-suspend",
                       test_rpmi_syssusp_attrs_and_suspend);
        qtest_add_func("/riscv/rpmi/cppc/hart-list",
                       test_rpmi_cppc_hart_list);
        qtest_add_func("/riscv/rpmi/cppc/read-nominal-perf",
                       test_rpmi_cppc_read_nominal_perf);
        qtest_add_func("/riscv/rpmi/cppc/invalid-hart",
                       test_rpmi_cppc_invalid_hart);
        qtest_add_func("/riscv/rpmi/cppc/fast-channel-region",
                       test_rpmi_cppc_fast_channel_region);
        qtest_add_func("/riscv/rpmi/cppc/fast-channel-offset",
                       test_rpmi_cppc_fast_channel_offset);
        qtest_add_func("/riscv/rpmi/cppc/counters-and-depth",
                       test_rpmi_cppc_counters_and_depth);
        qtest_add_func("/riscv/rpmi/cppc/fast-channel-perf-update",
                       test_rpmi_cppc_fast_channel_perf_update);
        qtest_add_func("/riscv/rpmi/cppc/write-reg-denied",
                       test_rpmi_cppc_write_reg_denied);
        qtest_add_func("/riscv/rpmi/sysmsi/attrs",
                       test_rpmi_sysmsi_attrs);
        qtest_add_func("/riscv/rpmi/sysmsi/rejects-bad-addr",
                       test_rpmi_sysmsi_rejects_bad_addr);
        qtest_add_func("/riscv/rpmi/sysmsi/msi-attrs-and-state",
                       test_rpmi_sysmsi_msi_attrs_and_state);
        qtest_add_func("/riscv/rpmi/sysmsi/powerdown-injects-pending",
                       test_rpmi_sysmsi_powerdown_injects_pending);
        qtest_add_func("/riscv/rpmi/sysmsi/reset-injects-pending",
                       test_rpmi_sysmsi_reset_injects_pending);
        qtest_add_func("/riscv/rpmi/sysmsi/suspend-injects-pending",
                       test_rpmi_sysmsi_suspend_injects_pending);
        qtest_add_func("/riscv/rpmi/sysmsi/target-roundtrip",
                       test_rpmi_sysmsi_target_roundtrip);
        qtest_add_func("/riscv/rpmi/clock/commands",
                       test_rpmi_clock_commands);
        qtest_add_func("/riscv/rpmi/clock/rates",
                       test_rpmi_clock_rates);
        qtest_add_func("/riscv/rpmi/mm/attrs",
                       test_rpmi_mm_attrs);
        qtest_add_func("/riscv/rpmi/mm/variable-roundtrip",
                       test_rpmi_mm_variable_roundtrip);
        qtest_add_func("/riscv/rpmi/mm/process-restart-persistence",
                       test_rpmi_mm_process_restart_persistence);
        qtest_add_func("/riscv/rpmi/mm/rejects-bad-range",
                       test_rpmi_mm_rejects_bad_range);
        qtest_add_func("/riscv/rpmi/mm/rejects-bad-store",
                       test_rpmi_mm_rejects_bad_store);
#ifdef CONFIG_LIBRPMI_LOGGING
        qtest_add_func("/riscv/rpmi/logging/set-config",
                       test_rpmi_logging_set_config);
#endif
    }

    return g_test_run();
}
