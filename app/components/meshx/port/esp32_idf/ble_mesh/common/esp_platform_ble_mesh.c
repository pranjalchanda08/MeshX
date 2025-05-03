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

    *p_model = (MESHX_MODEL*) MESHX_CALOC(nmax, sizeof(MESHX_MODEL));
    if(!*p_model)
        return MESHX_NO_MEM;

    *p_pub   = (MESHX_MODEL_PUB *) MESHX_CALOC(nmax, sizeof(MESHX_MODEL_PUB));
    if(!*p_pub)
        return MESHX_NO_MEM;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_del_model_pub(void ** p_model, void ** p_pub)
{
    if (!p_model || !p_pub)
        return MESHX_INVALID_ARG;

    if (*p_model) {
        MESHX_FREE(*p_model);
        *p_model = NULL;
    }

    if (*p_pub) {
        MESHX_FREE(*p_pub);
        *p_pub = NULL;
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_get_model_id(void* p_model, uint16_t *model_id)
{
    if(!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL * model = (MESHX_MODEL *)p_model;
    *model_id = model->model_id;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_create_plat_composition(void** p_comp)
{
    if(!p_comp)
        return MESHX_INVALID_ARG;

    *p_comp = (MESHX_COMPOSITION *) MESHX_MALLOC (sizeof(MESHX_COMPOSITION));
    if(!*p_comp)
        return MESHX_NO_MEM;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_add_element_to_composition(
    uint16_t index,
    void* p_element_list,
    void* p_sig_models,
    void* p_ven_models,
    uint8_t sig_cnt,
    uint8_t ven_cnt
) {
    if (!p_element_list) {
        return MESHX_INVALID_ARG;
    }

    MESHX_ELEMENT* element = (MESHX_ELEMENT*)(p_element_list + index);
    element->sig_models = p_sig_models;
    element->vnd_models = p_ven_models;

    memcpy((void*)&(element->sig_model_count), &sig_cnt, sizeof(uint8_t));
    memcpy((void*)&(element->vnd_model_count), &ven_cnt, sizeof(uint8_t));

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_composition_init(
    void* p_composition,
    void* p_elements,
    uint16_t cid,
    uint16_t pid,
    uint16_t element_idx
)
{
    if(!p_composition)
        return MESHX_INVALID_ARG;

    MESHX_COMPOSITION * composition = (MESHX_COMPOSITION *) p_composition;
    composition->cid = cid;
    composition->pid = pid;
    composition->element_count = element_idx;
    composition->elements = p_elements;

    return MESHX_SUCCESS;
}
