#ifndef __MESHX_PLAT_SRV_CMN_H__
#define __MESHX_PLAT_SRV_CMN_H__

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
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication structure to be created.
 * @param[in] nmax Maximum number of elements for the model and publication.
 *
 * @return MESHX_SUCCESS on successful allocation and initialization,
 *         MESHX_INVALID_ARG if any input pointer is NULL,
 *         MESHX_NO_MEM if memory allocation fails.
 */
meshx_err_t meshx_plat_create_model_pub(void ** p_model, void ** p_pub, uint16_t nmax);

meshx_err_t meshx_plat_del_model_pub(void ** p_model, void ** p_pub);

#endif /* __MESHX_PLAT_SRV_CMN_H__ */
