/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nvs_plat_zephyr.c
 * @brief Platform-specific implementation for Non-Volatile Storage (NVS) using Zephyr Settings / NVS.
 *        Implements RAM staging for writes and batch atomic commit to prevent flash wear.
 *
 * @author Pranjal Chanda
 */

#include "interface/utils/meshx_nvs_interface.h"
#include "interface/logging/meshx_log.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include <string.h>

#include <zephyr/kernel.h>
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#if defined(CONFIG_NVS)
#include <zephyr/fs/nvs.h>
#endif

#define MESHX_SETTINGS_SUBTREE        "meshx"
#define MESHX_NVS_KEY_LENGTH_MAX      32U
#define MESHX_NVS_FULL_KEY_LENGTH_MAX 64U

static uintptr_t g_meshx_zephyr_nvs_handle = 0xAA55AA55;

/**
 * @brief Structure for staging key-value writes in RAM before physical commit.
 */
typedef struct zephyr_nvs_staged_entry {
    char                            key[MESHX_NVS_KEY_LENGTH_MAX];
    uint8_t                        *data;
    uint16_t                        len;
    bool                            is_delete;
    struct zephyr_nvs_staged_entry *next;
} zephyr_nvs_staged_entry_t;

static zephyr_nvs_staged_entry_t *s_staged_head = NULL;

static void meshx_zephyr_nvs_clear_staging(void)
{
    zephyr_nvs_staged_entry_t *cur = s_staged_head;
    while (cur != NULL)
    {
        zephyr_nvs_staged_entry_t *next = cur->next;
        if (cur->data != NULL)
        {
            MESHX_FREE(&cur->data);
        }
        MESHX_FREE(&cur);
        cur = next;
    }
    s_staged_head = NULL;
}

meshx_err_t meshx_nvs_plat_open(uintptr_t *p_nvs_handle)
{
    if (p_nvs_handle == NULL)
    {
        return MESHX_ERR_PLAT;
    }

#if defined(CONFIG_SETTINGS)
    int ret = settings_subsys_init();

    if (ret != 0 && ret != -EALREADY)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr settings_subsys_init failed: %d", ret);
        return MESHX_ERR_PLAT;
    }
#endif

    *p_nvs_handle = g_meshx_zephyr_nvs_handle;
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr NVS platform backend opened");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_close(uintptr_t p_nvs_handle)
{
    MESHX_UNUSED(p_nvs_handle);
    meshx_zephyr_nvs_clear_staging();
    return MESHX_SUCCESS;
}

#if defined(CONFIG_SETTINGS)
typedef struct settings_read_ctx
{
    const char  *target_key;
    uint8_t     *p_data;
    uint16_t     len;
    bool         found;
    ssize_t      read_bytes;
}settings_read_ctx_t;

static int meshx_settings_direct_cb(const char *key, size_t len, settings_read_cb read_cb,
                                    void *cb_arg, void *param)
{
    settings_read_ctx_t *ctx = (settings_read_ctx_t *)param;
    if (strcmp(key, ctx->target_key) == 0)
    {
        ctx->found = true;
        size_t to_read = (len < ctx->len) ? len : ctx->len;
        ctx->read_bytes = read_cb(cb_arg, ctx->p_data, to_read);
        return 1; /* Stop searching */
    }
    return 0; /* Continue searching */
}

static int meshx_settings_delete_cb(const char *key, size_t len, settings_read_cb read_cb,
                                    void *cb_arg, void *param)
{
    MESHX_UNUSED(len); MESHX_UNUSED(read_cb); MESHX_UNUSED(cb_arg); MESHX_UNUSED(param);

    char full_key[MESHX_NVS_FULL_KEY_LENGTH_MAX];
    snprintf(full_key, sizeof(full_key), "%s/%s", MESHX_SETTINGS_SUBTREE, key);
    settings_delete(full_key);
    return 0;
}
#endif

meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key, uint8_t *p_data, uint16_t len)
{
    MESHX_UNUSED(p_nvs_handle);
    if (key == NULL || p_data == NULL || len == 0)
    {
        return MESHX_ERR_PLAT;
    }

    /* Check RAM staged entries first */
    zephyr_nvs_staged_entry_t *cur = s_staged_head;
    while (cur != NULL)
    {
        if (strncmp(cur->key, key, MESHX_NVS_KEY_LENGTH_MAX - 1) == 0)
        {
            if (cur->is_delete)
            {
                return MESHX_NOT_FOUND;
            }
            if (cur->data == NULL || cur->len == 0)
            {
                return MESHX_NOT_FOUND;
            }
            uint16_t to_copy = (len < cur->len) ? len : cur->len;
            memcpy(p_data, cur->data, to_copy);
            return MESHX_SUCCESS;
        }
        cur = cur->next;
    }

#if defined(CONFIG_SETTINGS)
    struct settings_read_ctx ctx =
    {
        .target_key = key,
        .p_data = p_data,
        .len = len,
        .found = false,
        .read_bytes = 0,
    };

    int ret = settings_load_subtree_direct(MESHX_SETTINGS_SUBTREE, meshx_settings_direct_cb, &ctx);
    if (ret != 0 || !ctx.found || ctx.read_bytes <= 0)
    {
        return MESHX_NOT_FOUND;
    }

    return MESHX_SUCCESS;
#else
    return MESHX_NOT_FOUND;
#endif
}

meshx_err_t meshx_nvs_plat_write(uintptr_t p_nvs_handle, char const *key, uint8_t const *p_data, uint16_t len)
{
    MESHX_UNUSED(p_nvs_handle);
    if (key == NULL || p_data == NULL || len == 0)
    {
        return MESHX_ERR_PLAT;
    }

    /* Check if key already exists in RAM staging buffer */
    zephyr_nvs_staged_entry_t *cur = s_staged_head;
    while (cur != NULL)
    {
        if (strncmp(cur->key, key, MESHX_NVS_KEY_LENGTH_MAX - 1) == 0)
        {
            if (cur->len != len || cur->data == NULL)
            {
                if (cur->data != NULL)
                {
                    MESHX_FREE(&cur->data);
                }
                meshx_err_t err = MESHX_MALLOC(&cur->data, len);
                if (err != MESHX_SUCCESS || cur->data == NULL)
                {
                    cur->len = 0;
                    return MESHX_NO_MEM;
                }
            }
            memcpy(cur->data, p_data, len);
            cur->len = len;
            cur->is_delete = false;
            return MESHX_SUCCESS;
        }
        cur = cur->next;
    }

    /* Allocate new staging node using MESHX_MALLOC */
    zephyr_nvs_staged_entry_t *node = NULL;
    meshx_err_t err = MESHX_MALLOC(&node, sizeof(zephyr_nvs_staged_entry_t));
    if (err != MESHX_SUCCESS || node == NULL)
    {
        return MESHX_NO_MEM;
    }
    memset(node, 0, sizeof(zephyr_nvs_staged_entry_t));
    strncpy(node->key, key, MESHX_NVS_KEY_LENGTH_MAX - 1);
    node->key[MESHX_NVS_KEY_LENGTH_MAX - 1] = '\0';

    err = MESHX_MALLOC(&node->data, len);
    if (err != MESHX_SUCCESS || node->data == NULL)
    {
        MESHX_FREE(&node);
        return MESHX_NO_MEM;
    }

    memcpy(node->data, p_data, len);
    node->len = len;
    node->is_delete = false;
    node->next = s_staged_head;
    s_staged_head = node;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle)
{
    MESHX_UNUSED(p_nvs_handle);
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr NVS erase requested");

    meshx_zephyr_nvs_clear_staging();

#if defined(CONFIG_SETTINGS)
    settings_load_subtree_direct(MESHX_SETTINGS_SUBTREE, meshx_settings_delete_cb, NULL);
#endif
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const *key)
{
    MESHX_UNUSED(p_nvs_handle);
    if (key == NULL)
    {
        return MESHX_ERR_PLAT;
    }

    /* If key exists in RAM staging buffer, update it as deleted */
    zephyr_nvs_staged_entry_t *cur = s_staged_head;
    while (cur != NULL)
    {
        if (strncmp(cur->key, key, MESHX_NVS_KEY_LENGTH_MAX - 1) == 0)
        {
            if (cur->data != NULL)
            {
                MESHX_FREE(&cur->data);
            }
            cur->data = NULL;
            cur->len = 0;
            cur->is_delete = true;
            return MESHX_SUCCESS;
        }
        cur = cur->next;
    }

    /* Key not in staging buffer, allocate a deletion tombstone node */
    zephyr_nvs_staged_entry_t *node = NULL;
    meshx_err_t err = MESHX_MALLOC(&node, sizeof(zephyr_nvs_staged_entry_t));
    if (err != MESHX_SUCCESS || node == NULL)
    {
        return MESHX_NO_MEM;
    }
    memset(node, 0, sizeof(zephyr_nvs_staged_entry_t));
    strncpy(node->key, key, MESHX_NVS_KEY_LENGTH_MAX - 1);
    node->key[MESHX_NVS_KEY_LENGTH_MAX - 1] = '\0';
    node->data = NULL;
    node->len = 0;
    node->is_delete = true;
    node->next = s_staged_head;
    s_staged_head = node;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle)
{
    MESHX_UNUSED(p_nvs_handle);
    meshx_err_t overall_err = MESHX_SUCCESS;

    zephyr_nvs_staged_entry_t *cur = s_staged_head;
    while (cur != NULL)
    {
        zephyr_nvs_staged_entry_t *next = cur->next;

#if defined(CONFIG_SETTINGS)
        char full_key[MESHX_NVS_FULL_KEY_LENGTH_MAX];
        snprintf(full_key, sizeof(full_key), "%s/%s", MESHX_SETTINGS_SUBTREE, cur->key);

        if (cur->is_delete)
        {
            int ret = settings_delete(full_key);
            if (ret != 0)
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr settings_delete failed for %s: %d", full_key, ret);
                overall_err = MESHX_ERR_PLAT;
            }
        }
        else if (cur->data != NULL && cur->len > 0)
        {
            int ret = settings_save_one(full_key, cur->data, cur->len);
            if (ret != 0)
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr settings_save_one failed for %s: %d", full_key, ret);
                overall_err = MESHX_ERR_PLAT;
            }
        }
#endif

        if (cur->data != NULL)
        {
            MESHX_FREE(&cur->data);
        }
        MESHX_FREE(&cur);
        cur = next;
    }

    s_staged_head = NULL;
    return overall_err;
}
