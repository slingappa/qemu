/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QTests for virt machine RPMI support.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv-rpmi-test.h"

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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    if (!qtest_has_machine("virt")) {
        return g_test_run();
    }

    qtest_add_func("/riscv/rpmi/virt/machine/realize-off",
                   test_rpmi_machine_realize_off);
    qtest_add_func("/riscv/rpmi/virt/machine/rejects-too-many-harts",
                   test_rpmi_machine_rejects_too_many_harts);
    rpmi_register_common_tests("/riscv/rpmi/virt", &virt_rpmi_machine,
                               RPMI_QTEST_ALL);

    return g_test_run();
}
