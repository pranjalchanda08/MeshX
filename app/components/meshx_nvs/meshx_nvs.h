/**
 * @file meshx_nvs.h
 * @brief Header file for MeshX Non-Volatile Storage (NVS) operations.
 *
 * This file provides APIs to manage the Non-Volatile Storage (NVS) used in the MeshX system.
 * It includes functions to read, write, erase, and manage key-value pairs stored persistently.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_NVS_H__
#define __MESHX_NVS_H__

#include <stdint.h>
#include "control_task.h"
#include "os_timer.h"
#include "nvs.h"

#define MESHX_NVS_TIMER_PERIOD_DEF  1000

#ifndef MESHX_NVS_TIMER_PERIOD
#define MESHX_NVS_TIMER_PERIOD      MESHX_NVS_TIMER_PERIOD_DEF
#endif /* MESHX_NVS_TIMER_PERIOD */


typedef struct meshx_nvs {
    uint16_t init;
    nvs_handle_t meshx_nvs_handle;
#ifdef MESHX_NVS_TIMER_PERIOD
    os_timer_t *meshx_nvs_stability_timer;
#endif /* MESHX_NVS_TIMER_PERIOD */
}meshx_nvs_t;

/**
 * @brief Erase all key-value pairs stored in the NVS.
 *
 * This function clears all data stored in the Non-Volatile Storage.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_erase(void);

/**
 * @brief Commit changes to the NVS.
 *
 * This function ensures that any pending changes to the NVS are flushed to persistent storage.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_commit(void);

/**
 * @brief Close the NVS handle.
 *
 * This function releases any resources associated with the NVS handle.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_close(void);

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
esp_err_t meshx_nvs_remove(char const* key);

/**
 * @brief Open the NVS with a timeout.
 *
 * This function initializes the NVS and sets a timeout for stability operations.
 *
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_open(void);

/**
 * @brief Get a value from the NVS.
 *
 * This function retrieves a value associated with the given key from the NVS.
 *
 * @param[in]       key        The key identifying the value to be retrieved.
 * @param[out]      blob       Pointer to the buffer where the value will be stored.
 * @param[inout]    blob_size  Size of the buffer in bytes.
 *
 * @return
 *  - ESP_OK: Success.
 */
esp_err_t meshx_nvs_get(char const* key, uint8_t* blob, size_t *blob_size);

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
esp_err_t meshx_nvs_set(char const* key, uint8_t const* blob, size_t blob_size, bool arm_timer);

#endif /* __MESHX_NVS_H__ */

