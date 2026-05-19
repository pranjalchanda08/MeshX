/**
 * @file meshx_builder_api.h
 * @brief C-compatible API for the MeshX Composition Builder.
 * 
 * This header provides a bridge for C code (like main.c and meshx.c) to 
 * interact with the C++ fluent builder pattern.
 * 
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_BUILDER_API_H__
#define __MESHX_BUILDER_API_H__

#include <meshx_common.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if a dynamic composition has been built.
 * @return true if elements have been added via the builder, false otherwise.
 */
bool meshx_builder_is_active(void);

/**
 * @brief Checks if TXCM should be enabled based on the composition.
 * @return true if any client elements were added, false otherwise.
 */
bool meshx_builder_is_txcm_active(void);

/**
 * @brief Bakes the dynamic composition into the device structure.
 * @param pdev Pointer to the device structure.
 * @param cid Company ID.
 * @param pid Product ID.
 * @param vid Version ID.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_builder_bake(dev_struct_t *pdev, uint16_t cid, uint16_t pid, uint16_t vid);

/**
 * @brief Adds elements of a specific type to the composition.
 * @param type The type of element to add.
 * @param count The number of elements of this type to add.
 */
void meshx_builder_add_element(meshx_element_type_t type, uint16_t count);

/**
 * @brief C-friendly builder lifecycle functions
 */
void meshx_builder_begin(void);
void meshx_builder_commit(void);

/**
 * @brief Retrieves element composition data into a byte buffer for serial transmission.
 * @param buf Output buffer
 * @param max_len Maximum length of the output buffer
 * @return Number of bytes written
 */
size_t meshx_get_element_composition_data(uint8_t *buf, size_t max_len);

/**
 * @brief Retrieves current element state data into a byte buffer for serial transmission.
 *
 * Format per element:
 *   [element_idx:u16][element_variant:u16][ctx_size:u16][ctx_data:ctx_size bytes]
 * First byte is the total element count.
 *
 * @param buf Output buffer
 * @param max_len Maximum length of the output buffer
 * @return Number of bytes written
 */
size_t meshx_get_element_state_data(uint8_t *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_BUILDER_API_H__ */
