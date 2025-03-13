#ifndef __MESHX_PLAT_SRV_CMN_H__
#define __MESHX_PLAT_SRV_CMN_H__

#include "meshx_err.h"
#include "meshx_platform.h"
#include "meshx_ble_mesh_cmn_def.h"

typedef struct meshx_model
{
    uint16_t el_id;
    uint16_t model_id;
    uint16_t pub_addr;
    void* p_model;
} meshx_model_t;

typedef struct meshx_ctx
{
    uint16_t src_addr;
    uint16_t dst_addr;
    uint32_t opcode;
    void * p_ctx;
} meshx_ctx_t;

meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr);

#endif /* __MESHX_PLAT_SRV_CMN_H__ */
