/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * RISC-V RPMI Management Mode service.
 *
 * Copyright (c) 2026 Qualcomm Technologies, Inc.
 * Author:
 *  Subrahmanya Lingappa <subrahmanya.lingappa@oss.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "riscv_rpmi_internal.h"
#include "qemu/bswap.h"
#include "qemu/log.h"
#include "system/block-backend-io.h"
#include "librpmi_mm_efi.h"

#define RISCV_RPMI_MM_FLASH_MAGIC 0x4d4d5052U /* "RPMM" */
#define RISCV_RPMI_MM_FLASH_FORMAT_VERSION 1
#define RISCV_RPMI_MM_FLASH_HEADER_SIZE 32
#define RISCV_RPMI_MM_FLASH_MAGIC_OFFSET 0
#define RISCV_RPMI_MM_FLASH_VERSION_OFFSET 4
#define RISCV_RPMI_MM_FLASH_LENGTH_OFFSET 8
#define RISCV_RPMI_MM_FLASH_CHECKSUM_OFFSET 12

void riscv_rpmi_mm_configure(RiscvRpmiState *s,
                              const RiscvRpmiConfig *cfg)
{
    s->mm_store_blk = cfg->mm_store_blk;
    s->mm_store_offset = cfg->mm_store_offset;
    s->mm_store_size = cfg->mm_store_size;
}

static uint64_t riscv_rpmi_mm_name_used_size(const uint16_t *name,
                                             uint64_t namesize)
{
    uint64_t chars = namesize / sizeof(uint16_t);

    for (uint64_t i = 0; i < chars; i++) {
        if (!name[i]) {
            return (i + 1) * sizeof(uint16_t);
        }
    }

    return 0;
}

static void riscv_rpmi_mm_variable_clear(RiscvRpmiMmVariable *var)
{
    if (!var) {
        return;
    }

    g_clear_pointer(&var->name, g_free);
    g_clear_pointer(&var->data, g_free);
    memset(&var->guid, 0, sizeof(var->guid));
    var->attr = 0;
    var->namesize = 0;
    var->datasize = 0;
    var->valid = false;
}

static void riscv_rpmi_mm_store_clear(RiscvRpmiState *s)
{
    if (!s->mm_variables) {
        return;
    }

    for (uint32_t i = 0; i < RISCV_RPMI_MM_MAX_VARIABLES; i++) {
        riscv_rpmi_mm_variable_clear(&s->mm_variables[i]);
    }
    s->mm_variable_count = 0;
}

static bool riscv_rpmi_mm_key_get_uint64(GKeyFile *keyfile,
                                         const char *group,
                                         const char *key,
                                         uint64_t *value,
                                         Error **errp)
{
    g_autoptr(GError) gerr = NULL;
    char *end = NULL;
    g_autofree char *str = g_key_file_get_value(keyfile, group, key, &gerr);

    if (!str) {
        error_setg(errp, "RPMI MM store missing %s/%s: %s", group, key,
                   gerr ? gerr->message : "unknown error");
        return false;
    }

    *value = g_ascii_strtoull(str, &end, 0);
    if (!end || *end) {
        error_setg(errp, "RPMI MM store has invalid %s/%s", group, key);
        return false;
    }

    return true;
}

static uint32_t riscv_rpmi_mm_store_checksum(const uint8_t *buf, size_t len)
{
    uint32_t checksum = 0;

    for (size_t i = 0; i < len; i++) {
        checksum = (checksum << 5) - checksum + buf[i];
    }

    return checksum;
}

static bool riscv_rpmi_mm_flash_is_empty(const uint8_t *buf, size_t len)
{
    bool all_zero = true;
    bool all_ones = true;

    for (size_t i = 0; i < len; i++) {
        all_zero &= buf[i] == 0;
        all_ones &= buf[i] == 0xff;
    }

    return all_zero || all_ones;
}

static bool riscv_rpmi_mm_keyfile_load_variables(RiscvRpmiState *s,
                                                 GKeyFile *keyfile,
                                                 Error **errp)
{
    uint64_t version;
    uint64_t count;

    if (!riscv_rpmi_mm_key_get_uint64(keyfile, "rpmi-mm", "version",
                                      &version, errp) ||
        !riscv_rpmi_mm_key_get_uint64(keyfile, "rpmi-mm", "count", &count,
                                      errp)) {
        return false;
    }

    if (version != RISCV_RPMI_MM_STORE_VERSION) {
        error_setg(errp, "unsupported RPMI MM store version %" PRIu64,
                   version);
        return false;
    }

    if (count > RISCV_RPMI_MM_MAX_VARIABLES) {
        error_setg(errp, "RPMI MM store has too many variables: %" PRIu64,
                   count);
        return false;
    }

    riscv_rpmi_mm_store_clear(s);
    for (uint64_t i = 0; i < count; i++) {
        g_autofree char *group = g_strdup_printf("variable-%" PRIu64, i);
        g_autoptr(GError) local_err = NULL;
        g_autofree char *name = NULL;
        g_autofree char *data = NULL;
        gsize namesize;
        gsize datasize;
        uint64_t attr;
        uint64_t namesize_key;
        uint64_t datasize_key;

        if (!riscv_rpmi_mm_key_get_uint64(keyfile, group, "attr", &attr,
                                          errp) ||
            !riscv_rpmi_mm_key_get_uint64(keyfile, group, "namesize",
                                          &namesize_key, errp) ||
            !riscv_rpmi_mm_key_get_uint64(keyfile, group, "datasize",
                                          &datasize_key, errp)) {
            return false;
        }

        name = (char *)g_key_file_get_string(keyfile, group, "name",
                                             &local_err);
        if (!name) {
            error_setg(errp, "RPMI MM store missing %s/name: %s", group,
                       local_err->message);
            return false;
        }
        data = (char *)g_key_file_get_string(keyfile, group, "data",
                                             &local_err);
        if (!data) {
            error_setg(errp, "RPMI MM store missing %s/data: %s", group,
                       local_err->message);
            return false;
        }

        RiscvRpmiMmVariable *var = &s->mm_variables[i];

        var->name = g_base64_decode(name, &namesize);
        var->data = g_base64_decode(data, &datasize);
        if (namesize != namesize_key || datasize != datasize_key ||
            namesize > UINT64_MAX || datasize > UINT64_MAX) {
            riscv_rpmi_mm_variable_clear(var);
            error_setg(errp, "RPMI MM store variable-%" PRIu64
                       " length mismatch", i);
            return false;
        }
        var->namesize = namesize;
        var->datasize = datasize;
        var->attr = attr;
        var->valid = true;

        for (uint32_t j = 0; j < sizeof(var->guid); j++) {
            g_autofree char *key = g_strdup_printf("guid%u", j);
            uint64_t byte;

            if (!riscv_rpmi_mm_key_get_uint64(keyfile, group, key, &byte,
                                              errp)) {
                return false;
            }
            if (byte > UINT8_MAX) {
                error_setg(errp, "RPMI MM store %s/%s is out of range",
                           group, key);
                return false;
            }
            ((uint8_t *)&var->guid)[j] = byte;
        }
        s->mm_variable_count++;
    }

    return true;
}

static bool riscv_rpmi_mm_store_load(RiscvRpmiState *s, Error **errp)
{
    g_autoptr(GKeyFile) keyfile = NULL;
    g_autoptr(GError) gerr = NULL;
    g_autofree uint8_t *header = NULL;
    g_autofree char *contents = NULL;
    int64_t blk_len;
    uint64_t length;
    uint32_t payload_len;
    uint32_t checksum;
    int ret;

    if (!s->mm_store_blk || !s->mm_store_size) {
        return true;
    }

    blk_len = blk_getlength(s->mm_store_blk);
    if (blk_len < 0) {
        error_setg_errno(errp, -blk_len, "failed to get RPMI MM flash size");
        return false;
    }
    length = blk_len;
    if (s->mm_store_offset > length ||
        s->mm_store_size > length - s->mm_store_offset) {
        error_setg(errp, "RPMI MM flash range is out of bounds");
        return false;
    }
    if (s->mm_store_size < RISCV_RPMI_MM_FLASH_HEADER_SIZE) {
        error_setg(errp, "RPMI MM flash is too small: %" PRIu64,
                   s->mm_store_size);
        return false;
    }

    header = g_malloc0(RISCV_RPMI_MM_FLASH_HEADER_SIZE);
    ret = blk_pread(s->mm_store_blk, s->mm_store_offset,
                    RISCV_RPMI_MM_FLASH_HEADER_SIZE, header, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "failed to read RPMI MM flash header");
        return false;
    }

    if (riscv_rpmi_mm_flash_is_empty(header,
                                     RISCV_RPMI_MM_FLASH_HEADER_SIZE)) {
        return true;
    }

    if (ldl_le_p(header + RISCV_RPMI_MM_FLASH_MAGIC_OFFSET) !=
        RISCV_RPMI_MM_FLASH_MAGIC) {
        error_setg(errp, "RPMI MM flash has invalid magic");
        return false;
    }
    if (ldl_le_p(header + RISCV_RPMI_MM_FLASH_VERSION_OFFSET) !=
        RISCV_RPMI_MM_FLASH_FORMAT_VERSION) {
        error_setg(errp, "RPMI MM flash has unsupported format version");
        return false;
    }

    payload_len = ldl_le_p(header + RISCV_RPMI_MM_FLASH_LENGTH_OFFSET);
    checksum = ldl_le_p(header + RISCV_RPMI_MM_FLASH_CHECKSUM_OFFSET);
    if (payload_len > s->mm_store_size - RISCV_RPMI_MM_FLASH_HEADER_SIZE) {
        error_setg(errp, "RPMI MM flash payload is out of range");
        return false;
    }
    if (!payload_len) {
        return true;
    }

    contents = g_malloc(payload_len + 1);
    ret = blk_pread(s->mm_store_blk,
                    s->mm_store_offset + RISCV_RPMI_MM_FLASH_HEADER_SIZE,
                    payload_len, contents, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "failed to read RPMI MM flash payload");
        return false;
    }
    contents[payload_len] = 0;
    if (riscv_rpmi_mm_store_checksum((uint8_t *)contents, payload_len) !=
        checksum) {
        error_setg(errp, "RPMI MM flash payload checksum mismatch");
        return false;
    }

    keyfile = g_key_file_new();
    if (!g_key_file_load_from_data(keyfile, contents, payload_len,
                                   G_KEY_FILE_NONE, &gerr)) {
        error_setg(errp, "failed to parse RPMI MM flash store: %s",
                   gerr->message);
        return false;
    }

    return riscv_rpmi_mm_keyfile_load_variables(s, keyfile, errp);
}

static uint64_t riscv_rpmi_mm_store_save(RiscvRpmiState *s)
{
    g_autoptr(GKeyFile) keyfile = NULL;
    g_autoptr(GError) gerr = NULL;
    g_autofree char *contents = NULL;
    g_autofree uint8_t *header = NULL;
    gsize length;
    int64_t blk_len;
    uint64_t flash_len;
    uint32_t index = 0;
    int ret;

    if (!s->mm_store_blk || !s->mm_store_size) {
        return EFI_SUCCESS;
    }

    keyfile = g_key_file_new();
    g_key_file_set_uint64(keyfile, "rpmi-mm", "version",
                          RISCV_RPMI_MM_STORE_VERSION);
    g_key_file_set_uint64(keyfile, "rpmi-mm", "count",
                          s->mm_variable_count);

    for (uint32_t i = 0; i < RISCV_RPMI_MM_MAX_VARIABLES; i++) {
        RiscvRpmiMmVariable *var = &s->mm_variables[i];
        g_autofree char *group = NULL;
        g_autofree char *name = NULL;
        g_autofree char *data = NULL;

        if (!var->valid) {
            continue;
        }

        group = g_strdup_printf("variable-%u", index++);
        name = g_base64_encode(var->name, var->namesize);
        data = g_base64_encode(var->data, var->datasize);
        g_key_file_set_uint64(keyfile, group, "attr", var->attr);
        g_key_file_set_uint64(keyfile, group, "namesize", var->namesize);
        g_key_file_set_uint64(keyfile, group, "datasize", var->datasize);
        g_key_file_set_string(keyfile, group, "name", name);
        g_key_file_set_string(keyfile, group, "data", data);
        for (uint32_t j = 0; j < sizeof(var->guid); j++) {
            g_autofree char *key = g_strdup_printf("guid%u", j);

            g_key_file_set_uint64(keyfile, group, key,
                                  ((uint8_t *)&var->guid)[j]);
        }
    }

    contents = g_key_file_to_data(keyfile, &length, &gerr);
    if (!contents) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "failed to serialize RPMI MM flash store: %s\n",
                      gerr->message);
        return EFI_ACCESS_DENIED;
    }

    blk_len = blk_getlength(s->mm_store_blk);
    if (blk_len < 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "failed to get RPMI MM flash size: %s\n",
                      strerror(-blk_len));
        return EFI_ACCESS_DENIED;
    }
    flash_len = blk_len;
    if (s->mm_store_offset > flash_len ||
        s->mm_store_size > flash_len - s->mm_store_offset) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMI MM flash range is out of bounds\n");
        return EFI_ACCESS_DENIED;
    }
    if (s->mm_store_size < RISCV_RPMI_MM_FLASH_HEADER_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "RPMI MM flash is too small\n");
        return EFI_OUT_OF_RESOURCES;
    }
    if (length > s->mm_store_size - RISCV_RPMI_MM_FLASH_HEADER_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "RPMI MM flash store is full\n");
        return EFI_OUT_OF_RESOURCES;
    }

    header = g_malloc0(RISCV_RPMI_MM_FLASH_HEADER_SIZE);
    stl_le_p(header + RISCV_RPMI_MM_FLASH_MAGIC_OFFSET,
             RISCV_RPMI_MM_FLASH_MAGIC);
    stl_le_p(header + RISCV_RPMI_MM_FLASH_VERSION_OFFSET,
             RISCV_RPMI_MM_FLASH_FORMAT_VERSION);
    stl_le_p(header + RISCV_RPMI_MM_FLASH_LENGTH_OFFSET, length);
    stl_le_p(header + RISCV_RPMI_MM_FLASH_CHECKSUM_OFFSET,
             riscv_rpmi_mm_store_checksum((uint8_t *)contents, length));

    ret = blk_pwrite(s->mm_store_blk,
                     s->mm_store_offset + RISCV_RPMI_MM_FLASH_HEADER_SIZE,
                     length, contents, 0);
    if (ret < 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "failed to write RPMI MM flash payload: %s\n",
                      strerror(-ret));
        return EFI_ACCESS_DENIED;
    }
    ret = blk_pwrite(s->mm_store_blk, s->mm_store_offset,
                     RISCV_RPMI_MM_FLASH_HEADER_SIZE, header, 0);
    if (ret < 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "failed to write RPMI MM flash header: %s\n",
                      strerror(-ret));
        return EFI_ACCESS_DENIED;
    }
    ret = blk_flush(s->mm_store_blk);
    if (ret < 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "failed to flush RPMI MM flash store: %s\n",
                      strerror(-ret));
        return EFI_ACCESS_DENIED;
    }

    return EFI_SUCCESS;
}

static RiscvRpmiMmVariable *
riscv_rpmi_mm_find_variable(RiscvRpmiState *s,
                             const struct rpmi_guid_t *guid,
                             const uint16_t *name, uint64_t namesize)
{
    if (!s->mm_variables || !guid || !name || !namesize) {
        return NULL;
    }

    for (uint32_t i = 0; i < RISCV_RPMI_MM_MAX_VARIABLES; i++) {
        RiscvRpmiMmVariable *var = &s->mm_variables[i];

        if (!var->valid || var->namesize != namesize) {
            continue;
        }

        if (memcmp(&var->guid, guid, sizeof(*guid)) ||
            memcmp(var->name, name, namesize)) {
            continue;
        }

        return var;
    }

    return NULL;
}

static RiscvRpmiMmVariable *
riscv_rpmi_mm_next_variable(RiscvRpmiState *s, RiscvRpmiMmVariable *var)
{
    uint32_t start = 0;

    if (!s->mm_variables) {
        return NULL;
    }

    if (var) {
        start = var - s->mm_variables + 1;
    }

    for (uint32_t i = start; i < RISCV_RPMI_MM_MAX_VARIABLES; i++) {
        if (s->mm_variables[i].valid) {
            return &s->mm_variables[i];
        }
    }

    return NULL;
}

static RiscvRpmiMmVariable *riscv_rpmi_mm_alloc_variable(RiscvRpmiState *s)
{
    if (s->mm_variable_count >= RISCV_RPMI_MM_MAX_VARIABLES) {
        return NULL;
    }

    for (uint32_t i = 0; i < RISCV_RPMI_MM_MAX_VARIABLES; i++) {
        if (!s->mm_variables[i].valid) {
            return &s->mm_variables[i];
        }
    }

    return NULL;
}

static uint64_t riscv_rpmi_mm_get_variable(void *priv,
                                           const rpmi_uint8_t *data,
                                           rpmi_uint32_t datasize)
{
    RiscvRpmiState *s = priv;
    struct efi_var_access_variable *request = (void *)data;
    RiscvRpmiMmVariable *var;
    uint64_t data_offset;

    if (!s || !data ||
        datasize < offsetof(struct efi_var_access_variable, name)) {
        return EFI_INVALID_PARAMETER;
    }

    var = riscv_rpmi_mm_find_variable(s, &request->guid, request->name,
                                      request->namesize);
    if (!var) {
        return EFI_NOT_FOUND;
    }

    request->attr = var->attr;
    if (request->datasize < var->datasize) {
        request->datasize = var->datasize;
        return EFI_BUFFER_TOO_SMALL;
    }

    data_offset = offsetof(struct efi_var_access_variable, name) +
                  request->namesize;
    if (data_offset > datasize || var->datasize > datasize - data_offset) {
        request->datasize = var->datasize;
        return EFI_BUFFER_TOO_SMALL;
    }

    memcpy((uint8_t *)request + data_offset, var->data, var->datasize);
    request->datasize = var->datasize;

    return EFI_SUCCESS;
}

static uint64_t riscv_rpmi_mm_get_next_variable_name(
    void *priv, const rpmi_uint8_t *data, rpmi_uint32_t datasize)
{
    RiscvRpmiState *s = priv;
    struct efi_var_get_next_var_name *request = (void *)data;
    RiscvRpmiMmVariable *var = NULL;
    uint64_t used_namesize;

    if (!s || !data ||
        datasize < offsetof(struct efi_var_get_next_var_name, name)) {
        return EFI_INVALID_PARAMETER;
    }

    used_namesize = riscv_rpmi_mm_name_used_size(request->name,
                                                 request->namesize);
    if (!used_namesize) {
        return EFI_INVALID_PARAMETER;
    }

    if (request->name[0]) {
        var = riscv_rpmi_mm_find_variable(s, &request->guid, request->name,
                                          used_namesize);
        if (!var) {
            return EFI_INVALID_PARAMETER;
        }
    }

    var = riscv_rpmi_mm_next_variable(s, var);
    if (!var) {
        return EFI_NOT_FOUND;
    }

    if (request->namesize < var->namesize) {
        request->namesize = var->namesize;
        return EFI_BUFFER_TOO_SMALL;
    }

    if (offsetof(struct efi_var_get_next_var_name, name) + var->namesize >
        datasize) {
        request->namesize = var->namesize;
        return EFI_BUFFER_TOO_SMALL;
    }

    request->guid = var->guid;
    request->namesize = var->namesize;
    memcpy(request->name, var->name, var->namesize);

    return EFI_SUCCESS;
}

static uint64_t riscv_rpmi_mm_set_variable(void *priv,
                                           const rpmi_uint8_t *data,
                                           rpmi_uint32_t datasize)
{
    RiscvRpmiState *s = priv;
    const struct efi_var_access_variable *request = (const void *)data;
    RiscvRpmiMmVariable *var;
    uint64_t data_offset;
    uint8_t *name;
    uint8_t *value;
    bool existed;

    if (!s || !data || !s->mm_variables ||
        datasize < offsetof(struct efi_var_access_variable, name)) {
        return EFI_INVALID_PARAMETER;
    }

    data_offset = offsetof(struct efi_var_access_variable, name) +
                  request->namesize;
    if (data_offset > datasize || request->datasize > datasize - data_offset) {
        return EFI_INVALID_PARAMETER;
    }

    var = riscv_rpmi_mm_find_variable(s, &request->guid, request->name,
                                      request->namesize);
    existed = !!var;
    if (!request->datasize) {
        if (!var) {
            return EFI_NOT_FOUND;
        }
        riscv_rpmi_mm_variable_clear(var);
        s->mm_variable_count--;
        return riscv_rpmi_mm_store_save(s);
    }

    if (!var) {
        var = riscv_rpmi_mm_alloc_variable(s);
        if (!var) {
            return EFI_OUT_OF_RESOURCES;
        }
    }

    name = g_memdup2(request->name, request->namesize);
    value = g_memdup2((const uint8_t *)request + data_offset,
                      request->datasize);
    riscv_rpmi_mm_variable_clear(var);
    var->guid = request->guid;
    var->attr = request->attr;
    var->namesize = request->namesize;
    var->datasize = request->datasize;
    var->name = name;
    var->data = value;
    var->valid = true;
    if (!existed) {
        s->mm_variable_count++;
    }

    return riscv_rpmi_mm_store_save(s);
}

static const struct rpmi_mm_efi_platform_ops riscv_rpmi_mm_efi_ops = {
    .get_variable = riscv_rpmi_mm_get_variable,
    .get_next_variable_name = riscv_rpmi_mm_get_next_variable_name,
    .set_variable = riscv_rpmi_mm_set_variable,
};

static bool riscv_rpmi_mm_create(RiscvRpmiState *s,
                                 struct rpmi_service_group **group,
                                 Error **errp)
{
    struct rpmi_mm_efi mm_efi = {
        .ops = &riscv_rpmi_mm_efi_ops,
        .ops_priv = s,
    };
    enum rpmi_error rc;

    s->mm_variables = g_new0(RiscvRpmiMmVariable,
                             RISCV_RPMI_MM_MAX_VARIABLES);
    if (!riscv_rpmi_mm_store_load(s, errp)) {
        g_clear_pointer(&s->mm_variables, g_free);
        return false;
    }

    *group = rpmi_service_group_mm_create(s->rpmi_shmem);
    if (!*group) {
        riscv_rpmi_mm_store_clear(s);
        g_clear_pointer(&s->mm_variables, g_free);
        error_setg(errp, "failed to create RPMI MM service group");
        return false;
    }

    rc = rpmi_mm_efi_register_service(*group, &mm_efi);
    if (rc != RPMI_SUCCESS) {
        rpmi_service_group_mm_destroy(*group);
        *group = NULL;
        riscv_rpmi_mm_store_clear(s);
        g_clear_pointer(&s->mm_variables, g_free);
        error_setg(errp, "failed to register RPMI MM EFI service: %d", rc);
        return false;
    }

    return true;
}

static void riscv_rpmi_mm_destroy(RiscvRpmiState *s)
{
    if (s->mm_group) {
        rpmi_service_group_mm_destroy(s->mm_group);
        s->mm_group = NULL;
    }

    if (s->mm_variables) {
        riscv_rpmi_mm_store_clear(s);
        g_clear_pointer(&s->mm_variables, g_free);
    }
}

bool riscv_rpmi_mm_add(RiscvRpmiState *s, Error **errp)
{
    struct rpmi_service_group *group;

    if (s->mm_group) {
        error_setg(errp, "duplicate RPMI MM service descriptor");
        return false;
    }

    if (!riscv_rpmi_mm_create(s, &group, errp)) {
        return false;
    }

    if (!riscv_rpmi_context_add_group(s, group, "MM", errp)) {
        s->mm_group = group;
        riscv_rpmi_mm_destroy(s);
        return false;
    }

    s->mm_group = group;
    return true;
}

void riscv_rpmi_mm_remove(RiscvRpmiState *s)
{
    riscv_rpmi_context_remove_group(s, s->mm_group);
    riscv_rpmi_mm_destroy(s);
}
