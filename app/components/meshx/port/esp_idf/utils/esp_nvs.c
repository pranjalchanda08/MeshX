/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file esp_nvs.c
 * @brief Implementation for ESP Non-Volatile Storage (NVS) drivers operations.
 *
 * This file provides APIs to manage the Non-Volatile Storage (NVS) used in the platform driver system.
 * It includes functions to read, write, erase, and manage key-value pairs stored persistently.
 *
 * @author Pranjal Chanda
 *
 */
#include "interface/utils/meshx_nvs_interface.h"
#include "interface/logging/meshx_log.h"
#include "module_id.h"
#include "esp_err.h"
#include "nvs.h"

#define MESHX_NVS_NAMESPACE         "MESHX_NVS"

/**
 * @brief      Open non-volatile storage with a given namespace from the default partition.
 *
 * If CONFIG_BLE_MESH_SPECIFIC_PARTITION is not defined, this function will open the namespace
 * from the default partition. Otherwise, it will open the namespace from the
 * MESHX_NVS_PARTITION.
 *
 * @param[out] p_nvs_handle   Pointer to the output variable populated with the opened handle.
 *
 * @return
 *             - MESHX_SUCCESS if the storage handle was opened successfully
 *             - MESHX_ERR_PLAT if p_nvs_handle is NULL
 */
meshx_err_t meshx_nvs_plat_open(uintptr_t *p_nvs_handle)
{
    if (p_nvs_handle == NULL)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid argument");
    }

    esp_err_t err = ESP_OK;
#ifndef CONFIG_BLE_MESH_SPECIFIC_PARTITION
    err = nvs_open(
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        (nvs_handle_t*)p_nvs_handle);
#else
    err = nvs_open_from_partition(
        MESHX_NVS_PARTITION,
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        (nvs_handle_t*)p_nvs_handle);
#endif /* CONFIG_BLE_MESH_SPECIFIC_PARTITION */

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to open NVS handle: %d", err);
        return MESHX_ERR_PLAT;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief      Close the non-volatile storage handle.
 *
 * @param[in]  p_nvs_handle   Handle obtained with meshx_nvs_plat_open.
 *
 * @return
 *             - MESHX_SUCCESS if the handle was closed successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_close(uintptr_t p_nvs_handle)
{

    nvs_close((nvs_handle_t)p_nvs_handle);

    return MESHX_SUCCESS;
}

/**
 * @brief      Read blob value for given key from non-volatile storage.
 *
 * @param[in]  p_nvs_handle   Handle obtained with meshx_nvs_plat_open.
 * @param[in]  key            Key name. Maximum length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[out] p_data         Pointer to the output variable populated with the read blob value.
 * @param[in]  len            Length of the read blob value.
 *
 * @return
 *             - MESHX_SUCCESS if the blob value was read successfully
 *             - MESHX_ERR_PLAT if there is an internal error;
 */
meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key , uint8_t *p_data, uint16_t len)
{
    esp_err_t err = ESP_OK;

    if (key == NULL || p_data == NULL || len == 0)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid argument");
        return MESHX_INVALID_ARG;
    }

    size_t read_len = len;
    err = nvs_get_blob((nvs_handle_t)p_nvs_handle, key, p_data, &read_len);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "nvs_get_blob failed");
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief      Write a blob value to the non-volatile storage with a given key and namespace.
 *
 * @param[in]  p_nvs_handle   Pointer to the opened handle.
 * @param[in]  key            Key name. Maximum length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  p_data         Pointer to the data to write.
 * @param[in]  len            Number of bytes to write.
 *
 * @return
 *             - MESHX_SUCCESS if the value was written successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_write(uintptr_t p_nvs_handle, char const * key, uint8_t const *p_data, uint16_t len)
{
    if (key == NULL || p_data == NULL || len == 0)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid argument");
        return MESHX_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;

    err = nvs_set_blob((nvs_handle_t)p_nvs_handle, key, p_data, len);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "nvs_set_blob failed");
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief      Erase all key-value pairs in the given namespace.
 *
 * @param[in]  p_nvs_handle   Pointer to the opened handle.
 *
 * @return
 *             - MESHX_SUCCESS if all key-value pairs were erased successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle)
{
    esp_err_t err = ESP_OK;

    err = nvs_erase_all((nvs_handle_t)p_nvs_handle);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "nvs_erase_all failed");
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief      Remove a key-value pair from the non-volatile storage with a given key and namespace.
 *
 * @param[in]  p_nvs_handle   Pointer to the opened handle.
 * @param[in]  key            Key name. Maximum length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 *
 * @return
 *             - MESHX_SUCCESS if the key-value pair was removed successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const* key)
{
    esp_err_t err = ESP_OK;

    err = nvs_erase_key((nvs_handle_t)p_nvs_handle, key);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "nvs_erase_key failed");
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief      Commit changes to the non-volatile storage.
 *
 * @note       This function is a prototype and can be returned with MESHX_SUCCESS if
 *             no such platform function exists
 *
 * @param[in]  p_nvs_handle   Pointer to the opened handle.
 *
 * @return
 *             - MESHX_SUCCESS if the changes were committed successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle)
{
    esp_err_t err = ESP_OK;

    err = nvs_commit((nvs_handle_t)p_nvs_handle);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "nvs_commit failed");
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}
