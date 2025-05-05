/**
 * @file meshx_ble_mesh_cmn.h
 * @brief Common BLE Mesh interface definitions and utility functions.
 *
 * This header file contains the definitions and function declarations for
 * managing BLE Mesh models, contexts, and compositions. It provides utilities
 * for checking model subscriptions, creating and deleting model structures,
 * retrieving model IDs, and managing BLE Mesh compositions.
 *
 * The functions defined here facilitate the initialization, manipulation, and
 * cleanup of BLE Mesh components, ensuring efficient memory management and
 * error handling.
 */
#ifndef __MESHX_PLAT_SRV_CMN_H__
#define __MESHX_PLAT_SRV_CMN_H__

#include "stdlib.h"
#include "string.h"
#include "meshx_err.h"
#include "interface/meshx_platform.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn_def.h"

typedef struct meshx_model
{
    uint16_t el_id;
    uint16_t model_id;
    uint16_t pub_addr;
    void* p_model;
} meshx_model_t;

typedef struct meshx_ctx
{
    uint16_t app_idx;
    uint16_t net_idx;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint32_t opcode;
    void * p_ctx;
} meshx_ctx_t;

/**
 * @brief Checks if a model is subscribed to a specific group address.
 *
 * This function determines whether the specified BLE Mesh model is subscribed
 * to a given group address by utilizing the internal function
 * esp_ble_mesh_is_model_subscribed_to_group.
 *
 * @param[in] p_model   Pointer to the BLE Mesh model structure.
 * @param[in] addr      The group address to check for subscription.
 *
 * @return MESHX_SUCCESS if the model is subscribed to the group address,
 *         otherwise MESHX_FAIL.
 */
meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr);

/**
 * @brief Creates and initializes model and publication structures.
 *
 * This function allocates memory for model and publication structures
 * based on the specified maximum number of elements. It initializes
 * the provided pointers to point to the newly allocated memory.
 *
 * @param[out] p_pub Pointer to the publication structure to be created.
 * @param[in] nmax Maximum number of elements for the model and publication.
 *
 * @return MESHX_SUCCESS on successful allocation and initialization,
 *         MESHX_INVALID_ARG if any input pointer is NULL,
 *         MESHX_NO_MEM if memory allocation fails.
 */
meshx_err_t meshx_plat_create_model_pub(void ** p_pub, uint16_t nmax);

/**
 * @brief Deletes the model and publication objects.
 *
 * This function frees the memory allocated for the model and publication
 * objects pointed to by the provided pointers and sets them to NULL.
 *
 * @param[in,out] p_pub Pointer to the publication object to be deleted.
 *
 * @return MESHX_SUCCESS on successful deletion, MESHX_INVALID_ARG if
 *         either pointer is NULL.
 */
meshx_err_t meshx_plat_del_model_pub(void ** p_pub);

/**
 * @brief Retrieve the model ID of a generic server model.
 *
 * This function obtains the model ID associated with a specified generic server model.
 *
 * @param[in]  p_model   Pointer to the model whose ID is to be retrieved.
 * @param[out] model_id  Pointer to a variable where the retrieved model ID will be stored.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_get_model_id(void* p_model, uint16_t *model_id);

/**
 * @brief Creates a platform-specific BLE Mesh composition object.
 *
 * This function allocates memory for a MESHX_COMPOSITION object and assigns
 * its pointer to the provided pointer argument. It checks for invalid
 * arguments and memory allocation failures, returning appropriate error codes.
 *
 * @param[out] p_comp Pointer to the location where the composition object
 *                    pointer will be stored.
 *
 * @return MESHX_SUCCESS on successful creation, MESHX_INVALID_ARG if the
 *         provided pointer is NULL, or MESHX_NO_MEM if memory allocation fails.
 */
meshx_err_t meshx_create_plat_composition(void** p_comp);

/**
 * @brief Adds an element to the BLE Mesh composition.
 *
 * This function adds a new element to the BLE Mesh composition at the specified
 * index. It assigns the provided SIG and vendor models to the element and sets
 * their respective counts.
 *
 * @param[in]       index           Index at which the element is to be added.
 * @param[in,out]   p_element_list  Pointer to the list of elements.
 * @param[in]       p_sig_models    Pointer to the SIG models to be assigned to the element.
 * @param[in]       p_ven_models    Pointer to the vendor models to be assigned to the element.
 * @param[in]       sig_cnt         Number of SIG models.
 * @param[in]       ven_cnt         Number of vendor models.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if the element list
 *         pointer is NULL.
 */
meshx_err_t meshx_plat_add_element_to_composition(
    uint16_t index,
    void* p_element_list,
    void* p_sig_models,
    void* p_ven_models,
    uint8_t sig_cnt,
    uint8_t ven_cnt
);

/**
 * @brief Initializes a platform-specific BLE Mesh composition.
 *
 * This function sets up a BLE Mesh composition object with the specified
 * company ID, product ID, and element index. It assigns the provided
 * elements to the composition.
 *
 * @param[out] p_composition Pointer to the composition object to be initialized.
 * @param[in]  p_elements    Pointer to the elements to be included in the composition.
 * @param[in]  cid           Company ID for the composition.
 * @param[in]  pid           Product ID for the composition.
 * @param[in]  element_idx   Index of the element within the composition.
 *
 * @return MESHX_SUCCESS on successful initialization, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_composition_init(
    void* p_composition,
    void* p_elements,
    uint16_t cid,
    uint16_t pid,
    uint16_t element_idx
);

#endif /* __MESHX_PLAT_SRV_CMN_H__ */
