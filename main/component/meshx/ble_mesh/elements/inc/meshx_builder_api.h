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
 * @brief Restores the NVS context for all elements in the active composition.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_restore_all_element_ctx(void);

/**
 * @brief Adds elements of a specific type to the composition.
 * @param type The type of element to add.
 * @param count The number of elements of this type to add.
 */
void meshx_builder_add_element(meshx_element_type_t type, uint16_t count);

/**
 * @brief Resets and starts a new dynamic composition build session.
 */
void meshx_builder_begin(void);

/**
 * @brief Clears all elements from the active composition builder session.
 */
void meshx_builder_clear(void);

/**
 * @brief Commits the pending dynamic composition build session.
 */
void meshx_builder_commit(void);

/**
 * @brief Saves the current dynamic composition schema to NVS.
 * @param override_ro true if this schema should override factory read-only config.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_builder_save_to_nvs(bool override_ro);

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

/**
 * @brief Get the number of elements in the active composition.
 * @return Total number of elements.
 */
uint8_t meshx_comp_get_element_count(void);

/**
 * @brief Get the variant of a specific element in the active composition.
 * @param element_idx Index of the element.
 * @return Element variant (e.g. MESHX_ELEMENT_TYPE_RELAY_SERVER), or 0 if invalid.
 */
uint16_t meshx_comp_get_element_variant(uint8_t element_idx);

/**
 * @brief Get the list of functional IDs supported by an element.
 * @param element_idx Index of the element.
 * @param out_func_ids Destination buffer to receive function IDs.
 * @param max_count Maximum number of function IDs to store.
 * @return Number of function IDs written.
 */
uint8_t meshx_comp_get_element_func_ids(uint8_t element_idx, uint16_t *out_func_ids, uint8_t max_count);

/**
 * @brief Check if a specific element contains a logical model with the given func_id.
 * @param element_idx Index of the element.
 * @param func_id Functional ID to query.
 * @return true if supported, false otherwise.
 */
bool meshx_comp_element_has_func_id(uint8_t element_idx, uint16_t func_id);

/**
 * @brief Set the publication address of an element.
 *
 * Updates the in-memory context and commits to NVS.
 *
 * @param[in] element_idx Index of the element.
 * @param[in] pub_addr    New publication address (unicast or group).
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_comp_set_element_pub_addr(uint8_t element_idx, uint16_t pub_addr);

/**
 * @brief Get the publication address of an element.
 *
 * @param[in] element_idx Index of the element.
 *
 * @return Publication address, or MESHX_ADDR_UNASSIGNED (0x0000) if unset.
 */
uint16_t meshx_comp_get_element_pub_addr(uint8_t element_idx);

/**
 * @brief Add a group subscription address to an element's models.
 *
 * @param[in] element_idx Index of the element.
 * @param[in] sub_addr    Group address to subscribe to (0xC000 - 0xC3FF).
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_comp_add_element_sub_addr(uint8_t element_idx, uint16_t sub_addr);

/**
 * @brief Delete a group subscription address from an element's models.
 *
 * @param[in] element_idx Index of the element.
 * @param[in] sub_addr    Group address to remove.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_comp_del_element_sub_addr(uint8_t element_idx, uint16_t sub_addr);

/**
 * @brief Send a UVP message from an element.
 *
 * @param[in] element_idx Element index to send from (or 0 for primary).
 * @param[in] dst_addr    Destination unicast, virtual, or group address.
 * @param[in] func_id     Function ID (e.g. MESHX_FUNC_ID_SYS_PAIRING).
 * @param[in] payload     Pointer to payload data.
 * @param[in] payload_len Length of payload.
 * @param[in] ack_req     True to request transport ACK.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_comp_send_uvp(uint8_t element_idx,
                                uint16_t dst_addr,
                                uint16_t func_id,
                                const void *payload,
                                uint16_t payload_len,
                                bool ack_req);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_BUILDER_API_H__ */
