/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_plat_ble_mesh.c
 * @brief Platform-specific implementation for BLE Mesh functionality on Zephyr RTOS.
 *        Provides composition, model publication, address resolution, and Bluetooth Mesh
 *        initialization using Zephyr native APIs (subsys/bluetooth/mesh).
 *
 * @author Pranjal Chanda
 */

#include <string.h>
#include "interface/meshx_platform.h"
#include "interface/logging/meshx_log.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"
#include "meshx_api.h"
#include "meshx_platform_ble_mesh.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif

uint16_t g_force_provisioned_addr = 0;
extern uint16_t bt_mesh_primary_addr(void);

meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr)
{
    if (p_model == NULL || p_model->p_model == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model->p_model;
    if (model->groups != NULL) {
        for (size_t i = 0; i < CONFIG_BT_MESH_MODEL_GROUP_COUNT; i++) {
            if (model->groups[i] == addr) {
                return MESHX_SUCCESS;
            }
        }
    }

    return MESHX_FAIL;
}

meshx_err_t meshx_plat_create_model_pub(void **p_pub, uint16_t nmax)
{
    if (!p_pub) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_CALLOC(p_pub, nmax, sizeof(MESHX_MODEL_PUB));
}

meshx_err_t meshx_plat_del_model_pub(void **p_pub)
{
    if (!p_pub) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_FREE(p_pub);
}

meshx_err_t meshx_get_model_id(meshx_ptr_t p_model, uint16_t *model_id)
{
    if (!p_model || !model_id) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    *model_id = model->id;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_create_plat_composition(meshx_ptr_t *p_comp)
{
    if (!p_comp) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_MALLOC(p_comp, sizeof(MESHX_COMPOSITION));
}

meshx_err_t meshx_plat_init_model_keys_groups(void *p_model)
{
    if (!p_model) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *m = (struct bt_mesh_model *)p_model;

    if (!m->rt) {
        struct bt_mesh_model_rt_ctx *rt = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&rt, 1, sizeof(struct bt_mesh_model_rt_ctx));
        if (err != MESHX_SUCCESS || !rt) {
            return MESHX_NO_MEM;
        }
        memcpy((void *)&m->rt, &rt, sizeof(rt));
    }

    if (!m->keys) {
        uint16_t *k = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&k, CONFIG_BT_MESH_MODEL_KEY_COUNT, sizeof(uint16_t));
        if (err != MESHX_SUCCESS || !k) {
            return MESHX_NO_MEM;
        }
        for (size_t i = 0; i < CONFIG_BT_MESH_MODEL_KEY_COUNT; i++) {
            k[i] = BT_MESH_KEY_UNUSED;
        }
        uint16_t cnt = CONFIG_BT_MESH_MODEL_KEY_COUNT;
        memcpy((void *)&m->keys, &k, sizeof(k));
        memcpy((void *)&m->keys_cnt, &cnt, sizeof(cnt));
    }

    if (!m->groups) {
        uint16_t *g = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&g, CONFIG_BT_MESH_MODEL_GROUP_COUNT, sizeof(uint16_t));
        if (err != MESHX_SUCCESS || !g) {
            return MESHX_NO_MEM;
        }
        for (size_t j = 0; j < CONFIG_BT_MESH_MODEL_GROUP_COUNT; j++) {
            g[j] = BT_MESH_ADDR_UNASSIGNED;
        }
        uint16_t cnt = CONFIG_BT_MESH_MODEL_GROUP_COUNT;
        memcpy((void *)&m->groups, &g, sizeof(g));
        memcpy((void *)&m->groups_cnt, &cnt, sizeof(cnt));
    }

#if defined(CONFIG_BT_MESH_LABEL_COUNT) && (CONFIG_BT_MESH_LABEL_COUNT > 0)
    if (!m->uuids) {
        const uint8_t **u = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&u, CONFIG_BT_MESH_LABEL_COUNT, sizeof(const uint8_t *));
        if (err != MESHX_SUCCESS || !u) {
            return MESHX_NO_MEM;
        }
        memcpy((void *)&m->uuids, &u, sizeof(u));
    }
#endif

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_init_element(void *p_elem)
{
    if (!p_elem) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_elem *element = (struct bt_mesh_elem *)p_elem;
    if (!element->rt) {
        struct bt_mesh_elem_rt_ctx *rt = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&rt, 1, sizeof(struct bt_mesh_elem_rt_ctx));
        if (err != MESHX_SUCCESS || !rt) {
            return MESHX_NO_MEM;
        }
        memcpy((void *)&element->rt, &rt, sizeof(rt));
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_add_element_to_composition(
    uint16_t index,
    meshx_ptr_t p_element_list,
    meshx_ptr_t p_sig_models,
    meshx_ptr_t p_ven_models,
    uint8_t sig_cnt,
    uint8_t ven_cnt
) {
    if (!p_element_list) {
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT, "Zephyr Plat Add Element [%d]: SIG=%d, VEN=%d",
               index, sig_cnt, ven_cnt);

    struct bt_mesh_elem *element = ((struct bt_mesh_elem *)p_element_list) + index;
    if (!element->rt) {
        struct bt_mesh_elem_rt_ctx *rt = NULL;
        meshx_err_t err = MESHX_CALLOC((void **)&rt, 1, sizeof(struct bt_mesh_elem_rt_ctx));
        if (err != MESHX_SUCCESS || !rt) {
            return MESHX_NO_MEM;
        }
        memcpy((void *)&element->rt, &rt, sizeof(rt));
    }
    MESHX_SET_ELEM_SIG_MODELS(element, p_sig_models);
    MESHX_SET_ELEM_SIG_COUNT(element, sig_cnt);
    MESHX_SET_ELEM_VND_MODELS(element, p_ven_models);
    MESHX_SET_ELEM_VND_COUNT(element, ven_cnt);

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_composition_init(
    meshx_ptr_t p_composition,
    meshx_ptr_t p_elements,
    uint16_t cid,
    uint16_t pid,
    uint16_t element_idx
) {
    if (!p_composition) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_comp *composition = (struct bt_mesh_comp *)p_composition;
    composition->cid = cid;
    composition->pid = pid;
    composition->vid = 0x0001;
    composition->elem_count = element_idx;
    composition->elem = (struct bt_mesh_elem *)p_elements;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_get_base_element_id(uint16_t *base_el_id)
{
    if (!base_el_id) {
        return MESHX_INVALID_ARG;
    }

    if (g_force_provisioned_addr != 0) {
        *base_el_id = g_force_provisioned_addr;
    } else {
        *base_el_id = bt_mesh_primary_addr();
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_platform_get_mac(uint8_t *mac)
{
    if (mac == NULL) {
        return MESHX_INVALID_ARG;
    }

    bt_addr_le_t addrs[1];
    size_t count = 1;
    bt_id_get(addrs, &count);

    if (count == 0) {
        int id = bt_id_create(NULL, NULL);
        if (id >= 0) {
            count = 1;
            bt_id_get(addrs, &count);
        }
    }

    if (count > 0) {
        memcpy(mac, addrs[0].a.val, MESHX_BD_ADDR_LEN);
        return MESHX_SUCCESS;
    }

#if defined(CONFIG_HWINFO)
    if (hwinfo_get_device_id(mac, MESHX_BD_ADDR_LEN) >= 0) {
        return MESHX_SUCCESS;
    }
#endif

    return MESHX_ERR_PLAT;
}

meshx_err_t meshx_platform_bt_init(void)
{
    int err = bt_enable(NULL);
    if (err && err != -EALREADY) {
        MESHX_LOGE(MODULE_ID_COMMON, "Zephyr bt_enable failed: %d", err);
        return MESHX_ERR_PLAT;
    }

#if defined(CONFIG_SETTINGS)
    settings_load();
#endif

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_ble_mesh_init(const meshx_prov_params_t *prov_cfg, meshx_ptr_t comp)
{
    if (comp == NULL || prov_cfg == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_prov *p_prov = (struct bt_mesh_prov *)meshx_plat_get_prov();
    if (p_prov == NULL) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get Zephyr provisioning instance");
        return MESHX_ERR_PLAT;
    }

    int err = bt_mesh_init(p_prov, (struct bt_mesh_comp *)comp);
    if (err) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize Zephyr mesh stack: %d", err);
        return MESHX_ERR_PLAT;
    }

#if defined(CONFIG_SETTINGS)
    settings_load();
#endif

    if (prov_cfg->node_name != NULL && prov_cfg->node_name[0] != '\0') {
#if defined(CONFIG_BT_DEVICE_NAME_DYNAMIC)
        int name_err = bt_set_name((const char *)prov_cfg->node_name);
        if (name_err) {
            MESHX_LOGW(MODULE_ID_MODEL_SERVER, "Failed to set Bluetooth device name to '%s': %d",
                       prov_cfg->node_name, name_err);
        } else {
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Bluetooth device name set to: %s", prov_cfg->node_name);
        }
#endif
    }

    meshx_err_t merr = meshx_plat_ven_srv_init();
    if (merr) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize vendor model pipeline");
        return merr;
    }

    if (bt_mesh_is_provisioned()) {
        uint16_t primary_addr = bt_mesh_primary_addr();
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Zephyr node already provisioned (primary addr: 0x%04x)", primary_addr);
        if (meshx_get_node_addr() == MESHX_ADDR_UNASSIGNED && primary_addr != MESHX_ADDR_UNASSIGNED) {
            meshx_prov_srv_param_t prov_srv_param;
            memset(&prov_srv_param, 0, sizeof(meshx_prov_srv_param_t));
            prov_srv_param.prov_evt = MESHX_NODE_PROV_COMPLETE_EVT;
            prov_srv_param.param.node_prov_complete.addr = primary_addr;
            meshx_prov_srv_notify_plat_event(&prov_srv_param);
        }
    } else {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Enabling Zephyr node unprovisioned beacons");
        bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
    }

    return MESHX_SUCCESS;
}
