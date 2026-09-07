/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QTests for riscv-server-ref RPMI support.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv-rpmi-test.h"

static void test_rpmi_rvserver_ref_rejects_rpmi_property(void)
{
    rpmi_expect_qemu_failure(
        "-machine riscv-server-ref,rpmi=off -display none -S",
        "Property 'riscv-server-ref-machine.rpmi' not found");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    if (!qtest_has_machine("riscv-server-ref")) {
        return g_test_run();
    }

    qtest_add_func("/riscv/rpmi/rvserver-ref/machine/rejects-rpmi-property",
                   test_rpmi_rvserver_ref_rejects_rpmi_property);
    rpmi_register_common_tests("/riscv/rpmi/rvserver-ref",
                               &rvserver_ref_rpmi_machine,
                               RPMI_QTEST_SERVER_REF);

    return g_test_run();
}
