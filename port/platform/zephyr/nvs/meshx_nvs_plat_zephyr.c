/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nvs_plat_zephyr.c
 * @brief Platform-specific implementation for Non-Volatile Storage (NVS) using Zephyr Settings / NVS.
 *        Implements the MeshX NVS interface layer without duplicating wear-leveling engines or FAL.
 *
 * @author Pranjal Chanda
 */

#include "interface/utils/meshx_nvs_interface.h"
#include "interface/logging/meshx_log.h"
#include <string.h>

#include <zephyr/kernel.h>
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#if defined(CONFIG_NVS)
#include <zephyr/fs/nvs.h>
#endif

#define MESHX_SETTINGS_SUBTREE   "meshx"
#define MESHX_NVS_KEY_LENGTH_MAX 32U

static uintptr_t g_meshx_zephyr_nvs_handle = 0xAA55AA55;

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
    ARG_UNUSED(p_nvs_handle);
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
    ARG_UNUSED(len); ARG_UNUSED(read_cb); ARG_UNUSED(cb_arg); ARG_UNUSED(param);

    char full_key[64];
    snprintf(full_key, sizeof(full_key), "%s/%s", MESHX_SETTINGS_SUBTREE, key);
    settings_delete(full_key);
    return 0;
}
#endif

meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key, uint8_t *p_data, uint16_t len)
{
    ARG_UNUSED(p_nvs_handle);
    if (key == NULL || p_data == NULL || len == 0)
    {
        return MESHX_ERR_PLAT;
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
    ARG_UNUSED(p_nvs_handle);
    if (key == NULL || p_data == NULL || len == 0)
    {
        return MESHX_ERR_PLAT;
    }

#if defined(CONFIG_SETTINGS)
    char full_key[MESHX_NVS_KEY_LENGTH_MAX];

    snprintf(full_key, sizeof(full_key), "%s/%s", MESHX_SETTINGS_SUBTREE, key);

    int ret = settings_save_one(full_key, p_data, len);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr settings_save_one failed for %s: %d", full_key, ret);
        return MESHX_ERR_PLAT;
    }
    return MESHX_SUCCESS;
#else
    return MESHX_SUCCESS;
#endif
}

meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle)
{
    ARG_UNUSED(p_nvs_handle);
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr NVS erase requested");
#if defined(CONFIG_SETTINGS)
    settings_load_subtree_direct(MESHX_SETTINGS_SUBTREE, meshx_settings_delete_cb, NULL);
#endif
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const *key)
{
    ARG_UNUSED(p_nvs_handle);
    if (key == NULL)
    {
        return MESHX_ERR_PLAT;
    }

#if defined(CONFIG_SETTINGS)
    char full_key[MESHX_NVS_KEY_LENGTH_MAX];
    snprintf(full_key, sizeof(full_key), "%s/%s", MESHX_SETTINGS_SUBTREE, key);

    int ret = settings_delete(full_key);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Zephyr settings_delete failed: %d", ret);
        return MESHX_ERR_PLAT;
    }
#endif

    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle)
{
    ARG_UNUSED(p_nvs_handle);
    /* In Zephyr settings_save_one commits synchronously to backend; return SUCCESS */
    return MESHX_SUCCESS;
}
