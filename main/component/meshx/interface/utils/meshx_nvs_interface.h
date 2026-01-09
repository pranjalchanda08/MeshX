/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file esp_nvs.c
 * @brief Implementation for Non-Volatile Storage (NVS) drivers Insterface APIs
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_NVS_INTERFACE_H__
#define __MESHX_NVS_INTERFACE_H__

#include "meshx_err.h"
#include <stdint.h>
#include <stddef.h>

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
meshx_err_t meshx_nvs_plat_open(uintptr_t *p_nvs_handle);

/**
 * @brief      Close the non-volatile storage handle.
 *
 * @param[in]  p_nvs_handle   Handle obtained with meshx_nvs_plat_open.
 *
 * @return
 *             - MESHX_SUCCESS if the handle was closed successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_close(uintptr_t p_nvs_handle);

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
meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key , uint8_t *p_data, uint16_t len);

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
meshx_err_t meshx_nvs_plat_write(uintptr_t p_nvs_handle, char const * key, uint8_t const *p_data, uint16_t len);

/**
 * @brief      Erase all key-value pairs in the given namespace.
 *
 * @param[in]  p_nvs_handle   Pointer to the opened handle.
 *
 * @return
 *             - MESHX_SUCCESS if all key-value pairs were erased successfully
 *             - MESHX_ERR_PLAT for any platform error
 */
meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle);

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
meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const* key);

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
meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle);

#endif /* __MESHX_NVS_INTERFACE_H__ */
