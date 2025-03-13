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
#include "meshx_control_task.h"
#include "meshx_os_timer.h"
#include "nvs.h"

#define MESHX_NVS_TIMER_PERIOD_DEF  1000

#ifndef MESHX_NVS_TIMER_PERIOD
#define MESHX_NVS_TIMER_PERIOD      MESHX_NVS_TIMER_PERIOD_DEF
#endif /* MESHX_NVS_TIMER_PERIOD */

#define MESHX_NVS_AUTO_COMMIT       true
#define MESHX_NVS_NO_AUTO_COMMIT    false

/**
 * @struct meshx_nvs
 * @brief Structure to hold the MeshX NVS data.
 */
typedef struct meshx_nvs {
    uint16_t init;                          /**< NVS initialization flag */
    uint16_t cid;                           /**< Company ID */
    uint16_t pid;                           /**< Product ID */
    nvs_handle_t meshx_nvs_handle;          /**< NVS handle */
#ifdef MESHX_NVS_TIMER_PERIOD
    meshx_os_timer_t *meshx_nvs_stability_timer;  /**< NVS stability timer */
#endif /* MESHX_NVS_TIMER_PERIOD */
}meshx_nvs_t;

/**
 * @brief MeshX NVS Initialisation
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_init(void);

/**
 * @brief Erase all key-value pairs stored in the NVS.
 *
 * This function clears all data stored in the Non-Volatile Storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_erase(void);

/**
 * @brief Commit changes to the NVS.
 *
 * This function ensures that any pending changes to the NVS are flushed to persistent storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_commit(void);

/**
 * @brief Close the NVS handle.
 *
 * This function releases any resources associated with the NVS handle.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_close(void);

/**
 * @brief Remove a key-value pair from the NVS.
 *
 * This function deletes a specific key-value pair from the NVS based on the provided key.
 *
 * @param[in] key The key identifying the value to be removed.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_remove(char const* key);

/**
 * @brief Open the NVS with a timeout.
 *
 * This function initializes the NVS and sets a timeout for stability operations.
 * @note NVS Namespace: MESHX_NVS_NAMESPACE
 *
 * @param[in] cid Company ID
 * @param[in] pid Product ID
 * @param[in] commit_timeout_ms Timeout for stability operations in milliseconds.
 *
 *
 * @note commit_timeout_ms = 0 -> use MESHX_NVS_TIMER_PERIOD
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_open(uint16_t cid, uint16_t pid, uint32_t commit_timeout_ms);

/**
 * @brief Get a value from the NVS.
 *
 * This function retrieves a value associated with the given key from the NVS.
 *
 * @param[in]    key         The key identifying the value to be retrieved.
 * @param[out]   blob        Pointer to the buffer where the value will be stored.
 * @param[in]    blob_size   Size of the buffer in bytes.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_get(char const *key, void *blob, size_t blob_size);

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
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_set(char const* key, void const* blob, size_t blob_size, bool arm_timer);

/**
 * @brief Retrieve the context of a specific element from NVS.
 *
 * This function fetches the stored context of a given element identified by its ID from
 * the Non-Volatile Storage (NVS).
 *
 * @param[in]   element_id  The ID of the element whose context is to be retrieved.
 * @param[out]  blob        Pointer to the buffer where the retrieved context will be stored.
 * @param[in]   blob_size   Size of the buffer provided to store the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully retrieved the context.
 */
meshx_err_t meshx_nvs_elemnt_ctx_get(uint16_t element_id, void *blob, size_t blob_size);

/**
 * @brief Store the context of a specific element to NVS.
 *
 * This function saves the context of a given element identified by its ID to
 * the Non-Volatile Storage (NVS).
 *
 * @param[in] element_id    The ID of the element whose context is to be stored.
 * @param[in] blob          Pointer to the buffer containing the context to be stored.
 * @param[in] blob_size     Size of the buffer containing the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully stored the context.
 */
meshx_err_t meshx_nvs_elemnt_ctx_set(uint16_t element_id, const void *blob, size_t blob_size);

#endif /* __MESHX_NVS_H__ */

