/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_kv_engine.h
 * @brief Portable Key-Value Engine for MeshX.
 *
 * This engine provides a lightweight KV store with built-in wear-leveling
 * and power-fail safety, designed to run on top of the FAL interface.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_KV_ENGINE_H__
#define __MESHX_KV_ENGINE_H__

#include "meshx_err.h"
#include "interface/utils/meshx_fal_interface.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the KV engine.
 *
 * @param[in] part Pointer to the flash partition to use.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_init(const meshx_fal_partition_t *part);

/**
 * @brief Read a value from the KV engine.
 *
 * @param[in]  key       Key name.
 * @param[out] buf       Buffer to store read data.
 * @param[in]  len       Length of the data to read.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_read(const char *key, void *buf, uint16_t len);

/**
 * @brief Buffer a write operation in RAM.
 *
 * @param[in] key       Key name.
 * @param[in] buf       Buffer containing data to write.
 * @param[in] len       Length of the data.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_set(const char *key, const void *buf, uint16_t len);

/**
 * @brief Commit all buffered changes to flash.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_commit(void);

/**
 * @brief Remove a key from the KV engine.
 *
 * @param[in] key Key name.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_remove(const char *key);

/**
 * @brief Erase the entire KV partition.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_kv_engine_erase_all(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_KV_ENGINE_H__ */
