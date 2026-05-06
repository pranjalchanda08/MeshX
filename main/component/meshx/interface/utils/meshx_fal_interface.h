/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_fal_interface.h
 * @brief Flash Abstraction Layer (FAL) Interface for MeshX.
 *
 * This header defines the interface for interacting with raw flash partitions.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_FAL_INTERFACE_H__
#define __MESHX_FAL_INTERFACE_H__

#include "meshx_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct meshx_fal_partition
 * @brief Structure representing a flash partition.
 */
typedef struct {
    char const *name;        /**< Name of the partition */
    uint32_t    base_addr;   /**< Base address of the partition (if applicable) */
    uint32_t    size;        /**< Total size of the partition in bytes */
    uint32_t    sector_size; /**< Size of an erasable sector in bytes */
    void       *priv;        /**< Platform-specific private data (e.g., partition handle) */
} meshx_fal_partition_t;

/**
 * @brief Read data from a flash partition.
 *
 * @param[in]  part    Pointer to the partition structure.
 * @param[in]  offset  Offset from the start of the partition.
 * @param[out] buf     Pointer to the buffer to store read data.
 * @param[in]  len     Number of bytes to read.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_fal_read(const meshx_fal_partition_t *part, uint32_t offset, void *buf, size_t len);

/**
 * @brief Write data to a flash partition.
 *
 * @param[in]  part    Pointer to the partition structure.
 * @param[in]  offset  Offset from the start of the partition.
 * @param[in]  buf     Pointer to the buffer containing data to write.
 * @param[in]  len     Number of bytes to write.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_fal_write(const meshx_fal_partition_t *part, uint32_t offset, const void *buf, size_t len);

/**
 * @brief Erase a range of a flash partition.
 *
 * @param[in]  part    Pointer to the partition structure.
 * @param[in]  offset  Offset from the start of the partition (must be sector aligned).
 * @param[in]  len     Number of bytes to erase (must be sector aligned).
 *
 * @return meshx_err_t MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_fal_erase(const meshx_fal_partition_t *part, uint32_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_FAL_INTERFACE_H__ */
