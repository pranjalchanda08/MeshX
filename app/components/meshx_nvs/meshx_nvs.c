/**
 * @file meshx_nvs.c
 * @brief Implementation for MeshX Non-Volatile Storage (NVS) operations.
 *
 * This file provides APIs to manage the Non-Volatile Storage (NVS) used in the MeshX system.
 * It includes functions to read, write, erase, and manage key-value pairs stored persistently.
 *
 * @author Pranjal Chanda
 *
 */
#include "meshx_nvs.h"

#define MESHX_NVS_INIT_MAGIC        0x5489
#define MESHX_NVS_NAMESPACE         "MESHX_NVS"
#define MESHX_NVS_TIMER_NAME        "MESHX_TIMER"
#define MESHX_NVS_RELOAD_ONE_SHOT   pdFALSE

/**
 * @brief: MeshX NVS Instance
 */
static meshx_nvs_t meshx_nvs_inst;

#if MESHX_NVS_TIMER_PERIOD
static void meshx_nvs_os_timer_cb(const os_timer_t *p_timer)
{
    esp_err_t err = ESP_OK;
    ESP_LOGD(TAG, "%s fire", p_timer->name);

    err = meshx_nvs_commit();
    if (err)
        ESP_LOGE(TAG, "meshx_nvs_commit %p", (void *)err);
}
#endif /* MESHX_NVS_TIMER_PERIOD */

/**
 * @brief Open the NVS with a timeout.
 *
 * This function initializes the NVS and sets a timeout for stability operations.
 * @note: NVS Namespace: MESHX_NVS_NAMESPACE
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_open(void)
{
    if (meshx_nvs_inst.init == MESHX_NVS_INIT_MAGIC)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;

#ifndef MESHX_NVS_PARTITION
    err = nvs_open(
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        &(meshx_nvs_inst.meshx_nvs_handle));
#else
    err = nvs_open_from_partition(
        MESHX_NVS_PARTITION,
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        &(meshx_nvs_inst.meshx_nvs_handle));
#endif /* MESHX_NVS_PARTITION */
    if (err)
    {
        ESP_LOGE(TAG, "nvs_open %p", (void *)err);
        return err;
    }

#if MESHX_NVS_TIMER_PERIOD
    err = os_timer_create(
        MESHX_NVS_TIMER_NAME,
        MESHX_NVS_TIMER_PERIOD,
        MESHX_NVS_RELOAD_ONE_SHOT,
        &meshx_nvs_os_timer_cb,
        &(meshx_nvs_inst.meshx_nvs_stability_timer));
    if (err)
    {
        ESP_LOGE(TAG, "os_timer_create %p", (void *)err);
        return err;
    }
#endif /* MESHX_NVS_TIMER_PERIOD */

    meshx_nvs_inst.init = MESHX_NVS_INIT_MAGIC;
    return err;
}

/**
 * @brief Erase all key-value pairs stored in the NVS.
 *
 * This function clears all data stored in the Non-Volatile Storage.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_erase(void)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return nvs_erase_all(meshx_nvs_inst.meshx_nvs_handle);
}

/**
 * @brief Commit changes to the NVS.
 *
 * This function ensures that any pending changes to the NVS are flushed to persistent storage.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_commit(void)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return nvs_commit(meshx_nvs_inst.meshx_nvs_handle);
}

/**
 * @brief Close the NVS handle.
 *
 * This function releases any resources associated with the NVS handle.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_close(void)
{

    esp_err_t err = ESP_OK;
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    nvs_close(meshx_nvs_inst.meshx_nvs_handle);

#if MESHX_NVS_TIMER_PERIOD
    err = os_timer_delete(&(meshx_nvs_inst.meshx_nvs_stability_timer));
#endif /* MESHX_NVS_TIMER_PERIOD */

    return err;
}

/**
 * @brief Remove a key-value pair from the NVS.
 *
 * This function deletes a specific key-value pair from the NVS based on the provided key.
 *
 * @param[in] key The key identifying the value to be removed.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_remove(char const *key)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return nvs_erase_key(meshx_nvs_inst.meshx_nvs_handle, key);
}

/**
 * @brief Get a value from the NVS.
 *
 * This function retrieves a value associated with the given key from the NVS.
 *
 * @param[in]    key         The key identifying the value to be retrieved.
 * @param[out]   blob        Pointer to the buffer where the value will be stored.
 * @param[inout] blob_size   Size of the buffer in bytes.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_get(char const *key, uint8_t *blob, size_t *blob_size)
{
    if (meshx_nvs_inst.init == MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return nvs_get_blob(meshx_nvs_inst.meshx_nvs_handle, key, blob, blob_size);
}

/**
 * @brief Set a value in the NVS.
 *
 * This function stores a value associated with the given key in the NVS.
 *
 * @param[in] key        The key identifying the value to be stored.
 * @param[in] blob       Pointer to the buffer containing the value.
 * @param[in] blob_size  Size of the buffer in bytes.
 * @param[in] arm_timer  Re-arm stability timer and auto commit
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_set(char const* key, uint8_t const* blob, size_t blob_size, bool arm_timer)
{
    if (meshx_nvs_inst.init == MESHX_NVS_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    if(arm_timer)
    {
        /* Trigger the stability timer to commit the changes */
        esp_err_t err = os_timer_restart(meshx_nvs_inst.meshx_nvs_stability_timer);
        if(err)
            ESP_LOGE(TAG, "os_timer_restart err:  %p", (void*) err);
    }

    return nvs_set_blob(meshx_nvs_inst.meshx_nvs_handle, key, blob, blob_size);
}
