#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"

meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr)
{
    const uint16_t * res = esp_ble_mesh_is_model_subscribed_to_group(p_model->p_model, addr);
    if(res != NULL)
        return MESHX_SUCCESS;

    return MESHX_FAIL;
}
meshx_err_t meshx_plat_create_model_pub(void ** p_model, void ** p_pub, uint16_t nmax)
{
    if(!p_model || !p_pub)
        return MESHX_INVALID_ARG;

    *p_model = (MESHX_MODEL*) calloc(nmax, sizeof(MESHX_MODEL));
    if(!*p_model)
        return MESHX_NO_MEM;

    *p_pub   = (MESHX_MODEL_PUB *) calloc(nmax, sizeof(MESHX_MODEL_PUB));
    if(!*p_pub)
        return MESHX_NO_MEM;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_del_model_pub(void ** p_model, void ** p_pub)
{
    if (!p_model || !p_pub)
        return MESHX_INVALID_ARG;

    if (*p_model) {
        free(*p_model);
        *p_model = NULL;
    }

    if (*p_pub) {
        free(*p_pub);
        *p_pub = NULL;
    }

    return MESHX_SUCCESS;
}
