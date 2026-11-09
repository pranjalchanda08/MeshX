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
#include <errno.h>
#include "interface/meshx_platform.h"
#include "interface/logging/meshx_log.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"
#include "meshx_api.h"
#include "meshx_platform_ble_mesh.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/cfg.h>
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#if defined(CONFIG_FLASH_MAP)
#include <zephyr/storage/flash_map.h>
#endif
#if defined(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif

#define MESHX_MAX_BLE_MFG_CBS 4

uint16_t g_force_provisioned_addr = 0;
extern uint16_t bt_mesh_primary_addr(void);

static meshx_plat_ble_mfg_data_cb_t s_plat_mfg_cbs[MESHX_MAX_BLE_MFG_CBS] = {0};
static size_t s_plat_mfg_cb_count = 0;
static struct bt_le_scan_cb s_plat_scan_cb;
static bool s_plat_scan_cb_registered = false;

/**
 * @brief Check if a model is subscribed to a group address.
 *
 * @param[in] p_model Pointer to the model instance.
 * @param[in] addr    Group address to check.
 * @return MESHX_SUCCESS if subscribed, MESHX_FAIL otherwise.
 */
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

/**
 * @brief Create a model publication context.
 *
 * @param[in]  p_pub  Pointer to publication context pointer.
 * @param[in]  nmax   Maximum number of publications.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null pointer.
 */
meshx_err_t meshx_plat_create_model_pub(void **p_pub, uint16_t nmax)
{
    if (!p_pub) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_CALLOC(p_pub, nmax, sizeof(MESHX_MODEL_PUB));
}

/**
 * @brief Delete a model publication context.
 *
 * @param[in] p_pub Pointer to publication context pointer.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null pointer.
 */
meshx_err_t meshx_plat_del_model_pub(void **p_pub)
{
    if (!p_pub) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_FREE(p_pub);
}

/**
 * @brief Get the model ID from a platform model instance.
 *
 * @param[in]  p_model   Pointer to the model instance.
 * @param[out] model_id  Output model ID.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null pointer.
 */
meshx_err_t meshx_get_model_id(meshx_ptr_t p_model, uint16_t *model_id)
{
    if (!p_model || !model_id) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    *model_id = model->id;

    return MESHX_SUCCESS;
}

/**
 * @brief Allocate memory for a platform composition.
 *
 * @param[out] p_comp Pointer to composition pointer.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null pointer.
 */
meshx_err_t meshx_create_plat_composition(meshx_ptr_t *p_comp)
{
    if (!p_comp) {
        return MESHX_INVALID_ARG;
    }

    return MESHX_MALLOC(p_comp, sizeof(MESHX_COMPOSITION));
}

/**
 * @brief Initialize runtime context, keys, and subscription groups for a baked Zephyr model.
 *
 * @param[in] p_model Pointer to the model instance.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_NO_MEM on failure.
 */
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

/**
 * @brief Initialize runtime context for a baked Zephyr element.
 *
 * @param[in] p_elem Pointer to the element instance.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_NO_MEM on failure.
 */
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

/**
 * @brief Add an element to the platform composition.
 *
 * @param[in] index      Element index.
 * @param[in] p_element_list Pointer to element list.
 * @param[in] p_sig_models Pointer to SIG model list.
 * @param[in] p_ven_models Pointer to vendor model list.
 * @param[in] sig_cnt    Number of SIG models.
 * @param[in] ven_cnt    Number of vendor models.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null element list.
 */
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

/**
 * @brief Initialize a platform composition structure.
 *
 * @param[in] p_composition Pointer to composition structure.
 * @param[in] p_elements    Pointer to element array.
 * @param[in] cid           Company ID.
 * @param[in] pid           Product ID.
 * @param[in] element_idx   Number of elements.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null composition.
 */
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

/**
 * @brief Get the base element ID (primary unicast address).
 *
 * @param[out] base_el_id Output base element ID.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null pointer.
 */
meshx_err_t meshx_get_base_element_id(uint16_t *base_el_id)
{
    if (!base_el_id) {
        return MESHX_INVALID_ARG;
    }

    if (g_force_provisioned_addr != 0) {
        *base_el_id = g_force_provisioned_addr;
    } else if (bt_mesh_is_provisioned()) {
        *base_el_id = bt_mesh_primary_addr();
    } else {
        *base_el_id = MESHX_ADDR_UNASSIGNED;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Get the Bluetooth device MAC address.
 *
 * @param[out] mac Output MAC address buffer.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_ERR_PLAT on failure.
 */
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

/**
 * @brief Initialize the Bluetooth controller and Zephyr settings.
 *
 * @return MESHX_SUCCESS on success, MESHX_ERR_PLAT on failure.
 */
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

/**
 * @brief Initialize the Zephyr BLE Mesh stack with provisioning and composition.
 *
 * @param[in] prov_cfg Pointer to provisioning parameters.
 * @param[in] comp     Pointer to composition structure.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
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

    if (!meshx_prov_srv_is_provisioned() && !bt_mesh_is_provisioned()) {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Enabling Zephyr node unprovisioned beacons");
        bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Zephyr unprovisioned beaconing enabled on boot");
    } else {
        uint16_t primary_addr = bt_mesh_primary_addr();
        if (primary_addr == MESHX_ADDR_UNASSIGNED) {
            primary_addr = meshx_get_node_addr();
        }
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Zephyr node already provisioned (primary addr: 0x%04x); unprovisioned beaconing gated on boot", primary_addr);
        if (meshx_get_node_addr() == MESHX_ADDR_UNASSIGNED && primary_addr != MESHX_ADDR_UNASSIGNED) {
            meshx_prov_srv_param_t prov_srv_param;
            memset(&prov_srv_param, 0, sizeof(meshx_prov_srv_param_t));
            prov_srv_param.prov_evt = MESHX_NODE_PROV_COMPLETE_EVT;
            prov_srv_param.param.node_prov_complete.addr = primary_addr;
            meshx_prov_srv_notify_plat_event(&prov_srv_param);
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Get the active Network Key from the Zephyr BLE Mesh stack.
 *
 * @param[in]  net_key_id NetKey Index.
 * @param[out] key_out    16-byte output buffer for the NetKey.
 * @return MESHX_SUCCESS on success, MESHX_NOT_SUPPORTED if not implemented.
 */
meshx_err_t meshx_plat_net_key_get(uint16_t net_key_id, uint8_t *key_out)
{
    if (!key_out) {
        return MESHX_INVALID_ARG;
    }

    /* Zephyr BLE Mesh does not expose a public API to retrieve a subnet's
     * net key by index. The net keys are managed internally by the stack.
     * This is a stub; implement via bt_mesh_sub[] internal access when
     * Zephyr net key retrieval is required. */
    MESHX_LOGW(MODULE_ID_COMMON, "meshx_plat_net_key_get: not implemented on Zephyr");
    return MESHX_NOT_SUPPORTED;
}

/**
 * @brief Add a Network Key to the Zephyr BLE Mesh stack.
 *
 * @param[in] key         16-byte NetKey.
 * @param[in] net_key_id  NetKey Index.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_FAIL on failure.
 */
meshx_err_t meshx_plat_net_key_add(const uint8_t *key, uint16_t net_key_id)
{
    if (!key) {
        return MESHX_INVALID_ARG;
    }

    /* Add subnet/net key via Zephyr BLE Mesh config API */
    uint8_t result = bt_mesh_subnet_add(net_key_id, key);
    if (result != 0) {
        MESHX_LOGE(MODULE_ID_COMMON, "bt_mesh_subnet_add failed: %d", result);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

#if !defined(CONFIG_BT_EXT_ADV) || !CONFIG_BT_EXT_ADV
#error "CONFIG_BT_EXT_ADV must be enabled for meshx_plat_ble_adv_mfg_data to broadcast full-length BLE advertising packets without truncation"
#endif

/**
 * @brief Advertise manufacturer-specific data via BLE extended advertising.
 *
 * @param[in] mfg_data Pointer to manufacturer data buffer.
 * @param[in] mfg_len  Length of manufacturer data in bytes.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_plat_ble_adv_mfg_data(const uint8_t *mfg_data, uint16_t mfg_len)
{
    if (!mfg_data || mfg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    static struct bt_le_ext_adv *s_ext_adv = NULL;

    if (s_ext_adv == NULL)
    {
        /* Use standard legacy non-connectable advertising on primary channels (37, 38, 39)
         * with BT_LE_ADV_OPT_USE_IDENTITY (BT_HCI_LE_ADV_PROP_LEGACY) so standard 1M BLE
         * sniffers and unprovisioned peers capture the beacon immediately. */
        const struct bt_le_adv_param legacy_param = {
            .options = BT_LE_ADV_OPT_USE_IDENTITY,
            .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
            .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
            .peer = NULL,
        };
        int err = bt_le_ext_adv_create(&legacy_param, NULL, &s_ext_adv);
        if (err != 0)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "bt_le_ext_adv_create failed: %d", err);
            return MESHX_FAIL;
        }
    }

    const struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, mfg_len),
    };

    int err = bt_le_ext_adv_set_data(s_ext_adv, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err != 0)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "bt_le_ext_adv_set_data failed: %d", err);
        return MESHX_FAIL;
    }

    err = bt_le_ext_adv_start(s_ext_adv, NULL);
    if (err != 0 && err != -EALREADY)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "bt_le_ext_adv_start failed: %d", err);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "plat_ble_adv_mfg_data: Advertised via Legacy Adv (len=%u)",
               (unsigned int)mfg_len);
    return MESHX_SUCCESS;
}

/**
 * @brief Parse advertising data and invoke registered manufacturer data callbacks.
 *
 * @param[in] data      Pointer to advertising data structure.
 * @param[in] user_data Pointer to RSSI value.
 * @return true to continue parsing, false to stop.
 */
static bool plat_scan_parse_ad(struct bt_data *data, void *user_data)
{
    int8_t rssi = *(int8_t *)user_data;
    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len >= 2)
    {
        uint16_t cid = data->data[0] | ((uint16_t)data->data[1] << 8);
        const uint8_t *payload = &data->data[2];
        uint16_t payload_len = data->data_len - 2;

        for (size_t i = 0; i < s_plat_mfg_cb_count; i++)
        {
            if (s_plat_mfg_cbs[i] != NULL)
            {
                s_plat_mfg_cbs[i](cid, payload, payload_len, rssi);
            }
        }
    }
    return true;
}

/**
 * @brief BLE scan receive callback. Parses advertising data and dispatches to manufacturer data callbacks.
 *
 * @param[in] info Pointer to scan receive info.
 * @param[in] buf  Pointer to advertising data buffer.
 */
static void plat_scan_recv_cb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    if (s_plat_mfg_cb_count == 0)
    {
        return;
    }
    int8_t rssi = info ? info->rssi : 0;
    struct net_buf_simple ad_buf;
    net_buf_simple_clone(buf, &ad_buf);
    bt_data_parse(&ad_buf, plat_scan_parse_ad, &rssi);
}

/**
 * @brief Register a manufacturer data callback for BLE scan observations.
 *
 * @param[in] cb Manufacturer data callback function.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on null callback, MESHX_NO_MEM if callback table full.
 */
meshx_err_t meshx_plat_ble_register_mfg_data_cb(meshx_plat_ble_mfg_data_cb_t cb)
{
    if (!cb)
    {
        return MESHX_INVALID_ARG;
    }

    for (size_t i = 0; i < s_plat_mfg_cb_count; i++)
    {
        if (s_plat_mfg_cbs[i] == cb)
        {
            return MESHX_SUCCESS; /* Already registered */
        }
    }

    if (s_plat_mfg_cb_count >= MESHX_MAX_BLE_MFG_CBS)
    {
        return MESHX_NO_MEM;
    }

    s_plat_mfg_cbs[s_plat_mfg_cb_count++] = cb;

    if (!s_plat_scan_cb_registered)
    {
        s_plat_scan_cb.recv = plat_scan_recv_cb;
        int err = bt_le_scan_cb_register(&s_plat_scan_cb);
        if (err == 0 || err == -EEXIST)
        {
            s_plat_scan_cb_registered = true;
            MESHX_LOGI(MODULE_ID_COMMON, "plat_ble: Registered scan observer for manufacturer data");
        }
        else
        {
            MESHX_LOGW(MODULE_ID_COMMON, "plat_ble: bt_le_scan_cb_register failed: %d", err);
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Platform self-provisioning wrapper for Case-2 Anchor nodes on Zephyr.
 *
 * Directly provisions the Zephyr BLE Mesh node stack with generated Network Key,
 * primary unicast address, and Device Key without requiring external provisioner link.
 *
 * @param[in] net_key  16-byte Network Key.
 * @param[in] net_idx  Network Key index (typically 0).
 * @param[in] addr     Assigned primary element unicast address (0x0001).
 * @param[in] dev_key  16-byte self Device Key.
 *
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on bad arguments, or MESHX_FAIL on error.
 */
meshx_err_t meshx_plat_self_provision(const meshx_key_t net_key, uint16_t net_idx, uint16_t addr, const meshx_key_t dev_key)
{
    if (!net_key || !dev_key || addr == MESHX_ADDR_UNASSIGNED)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t flags = 0;
    uint32_t iv_index = 0;

    int ret = bt_mesh_provision(net_key, net_idx, flags, iv_index, addr, dev_key);
    if (ret != 0 && ret != -EALREADY)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Zephyr bt_mesh_provision failed: %d", ret);
        return MESHX_FAIL;
    }

    /* Disable unprovisioned beacons */
    bt_mesh_prov_disable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

    /* Update internal forced primary address */
    g_force_provisioned_addr = addr;

    /* Notify Prov Server of completion */
    meshx_prov_srv_param_t prov_srv_param;
    memset(&prov_srv_param, 0, sizeof(meshx_prov_srv_param_t));
    prov_srv_param.prov_evt = MESHX_NODE_PROV_COMPLETE_EVT;
    prov_srv_param.param.node_prov_complete.addr    = addr;
    prov_srv_param.param.node_prov_complete.net_idx = net_idx;
    memcpy(prov_srv_param.param.node_prov_complete.net_key, net_key, MESHX_KEY_LEN);
    meshx_prov_srv_notify_plat_event(&prov_srv_param);

    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Zephyr platform self-provisioning complete: addr=0x%04X, net_idx=0x%04X", addr, net_idx);
    return MESHX_SUCCESS;
}

/**
 * @brief Add an Application Key to the local Zephyr BLE Mesh node stack.
 *
 * @param[in] key         16-byte Application Key buffer.
 * @param[in] net_key_id  Bound Network Key index.
 * @param[in] app_key_id  Application Key index to add.
 *
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on bad arguments, or MESHX_FAIL on error.
 */
meshx_err_t meshx_plat_app_key_add(const uint8_t *key, uint16_t net_key_id, uint16_t app_key_id)
{
    if (!key)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t status = bt_mesh_app_key_add(app_key_id, net_key_id, key);
    if (status != 0)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Zephyr bt_mesh_app_key_add failed: %d", (int)status);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Resets the underlying Zephyr BLE Mesh stack.
 *
 * Invokes native bt_mesh_reset() to clear all Network Keys, App Keys, sequence numbers,
 * IV index, and unbinds models. Resets forced primary address cache.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_plat_ble_mesh_reset(void)
{
    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Resetting Zephyr BLE Mesh stack via bt_mesh_reset()");
    bt_mesh_reset();
    g_force_provisioned_addr = 0;

#if defined(CONFIG_FLASH_MAP) && FIXED_PARTITION_EXISTS(storage_partition)
    const struct flash_area *fa = NULL;
    int rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    if (rc == 0 && fa != NULL)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Erasing Zephyr settings storage_partition (size: %u bytes)", (unsigned int)fa->fa_size);
        rc = flash_area_erase(fa, 0, fa->fa_size);
        if (rc != 0)
        {
            MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to erase storage_partition: %d", rc);
        }
        flash_area_close(fa);
    }
    else
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to open storage_partition: %d", rc);
    }
#endif

    return MESHX_SUCCESS;
}


