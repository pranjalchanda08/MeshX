#include "meshx_ble_mesh_cmn.h"

meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr)
{
    uint16_t * res = esp_ble_mesh_is_model_subscribed_to_group(p_model->p_model, addr);
    if(res != NULL)
        return MESHX_SUCCESS;

    return MESHX_FAIL;
}
