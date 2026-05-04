/**
 * @file meshx_composition.cpp
 * @brief Implementation of MeshX Dynamic Composition Manager.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <meshx_composition.hpp>
#include <meshx_element_factory.hpp>
#include <meshx_element_class.hpp>
#include <meshx_model_class.hpp>
#include <meshx_element_registry.hpp>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
#include <meshx_api.h>
#include <meshx_common.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h>

/** Forward declarations for legacy root model accessors */
MESHX_MODEL* get_root_sig_models(void);
MESHX_MODEL* get_root_ven_models(void);
size_t get_root_sig_models_count(void);
size_t get_root_ven_models_count(void);
meshx_err_t meshx_init_config_server(void);

#ifdef __cplusplus
}
#endif

meshXComposition& meshXComposition::get_instance() {
    static meshXComposition instance;
    return instance;
}

meshx_err_t meshXComposition::bake(uint16_t cid, uint16_t pid, uint16_t vid) {
    if (is_baked) return MESHX_SUCCESS;
    if (!pdev) {
        MESHX_LOGE(MODULE_ID_COMMON, "Composition bake failed: Device struct not set");
        return MESHX_INVALID_STATE;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "Baking composition with CID: 0x%04x, PID: 0x%04x, VID: 0x%04x, dynamic elements: %zu", cid, pid, vid, elements.size());

    size_t total_elements = elements.size() + 1; // +1 for root (index 0)

    /* Allocate and initialize platform structures */
    baked_elements.resize(total_elements * sizeof(MESHX_ELEMENT));
    memset(baked_elements.data(), 0, baked_elements.size());
    baked_sig_model_arrays.resize(total_elements);
    baked_ven_model_arrays.resize(total_elements);

    MESHX_ELEMENT* p_elements = reinterpret_cast<MESHX_ELEMENT*>(baked_elements.data());

    /* 1. Initialize Config Server (Legacy dependency) */
    meshx_err_t err = meshx_init_config_server();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to init config server: %d", err);
        return err;
    }

    /* 2. Bake Root Element (Index 0) */
    size_t root_sig_cnt = get_root_sig_models_count();
    size_t root_ven_cnt = get_root_ven_models_count();
    MESHX_MODEL* root_sig_ptr = get_root_sig_models();
    MESHX_MODEL* root_ven_ptr = get_root_ven_models();

    if (root_sig_cnt > 0 && root_sig_ptr) {
        baked_sig_model_arrays[0].resize(root_sig_cnt * sizeof(MESHX_MODEL));
        memcpy(baked_sig_model_arrays[0].data(), root_sig_ptr, root_sig_cnt * sizeof(MESHX_MODEL));
    }
    if (root_ven_cnt > 0 && root_ven_ptr) {
        baked_ven_model_arrays[0].resize(root_ven_cnt * sizeof(MESHX_MODEL));
        memcpy(baked_ven_model_arrays[0].data(), root_ven_ptr, root_ven_cnt * sizeof(MESHX_MODEL));
    }

    {
        uint16_t loc = 0;
        uint8_t sig_cnt = (uint8_t)root_sig_cnt;
        uint8_t ven_cnt = (uint8_t)root_ven_cnt;
        esp_ble_mesh_model_t* sig_ptr = (root_sig_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_sig_model_arrays[0].data()) : nullptr;
        esp_ble_mesh_model_t* ven_ptr = (root_ven_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_ven_model_arrays[0].data()) : nullptr;

        MESHX_ELEMENT* e = &p_elements[0];
        memcpy((void*)&e->location, &loc, sizeof(e->location));
        memcpy((void*)&e->sig_model_count, &sig_cnt, sizeof(e->sig_model_count));
        memcpy((void*)&e->vnd_model_count, &ven_cnt, sizeof(e->vnd_model_count));
        memcpy((void*)&e->sig_models, &sig_ptr, sizeof(e->sig_models));
        memcpy((void*)&e->vnd_models, &ven_ptr, sizeof(e->vnd_models));
    }

    /* 3. Bake Dynamic Elements (Indices 1 to N) */
    for (size_t i = 0; i < elements.size(); ++i) {
        size_t plat_idx = i + 1;
        auto& el = elements[i];

        /* Ensure models are listed and objects created */
        el->list_sig_models();
        el->list_ven_models();

        auto& sig_models = el->get_sig_models();
        auto& ven_models = el->get_ven_models();

        /* Populate SIG models */
        baked_sig_model_arrays[plat_idx].resize(sig_models.size() * sizeof(MESHX_MODEL));
        for (size_t m_idx = 0; m_idx < sig_models.size(); ++m_idx) {
            auto& m = sig_models[m_idx];
            m->set_parent_element(el.get());

            /* Get pointer to the baked slot for this model */
            MESHX_MODEL* p_baked = (MESHX_MODEL*)&baked_sig_model_arrays[plat_idx][m_idx * sizeof(MESHX_MODEL)];

            /* Create the platform model directly into the baked slot */
            if (m->plat_model_create(p_baked) != MESHX_SUCCESS) {
                MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT, "Failed to create platform model for SIG model %d in element %d", (int)m_idx, (int)plat_idx);
                continue;
            }

            if (m->get_plat_model() == NULL) {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "get_plat_model() returned NULL for SIG model %d after creation", (int)m_idx);
                continue;
            }
        }

        /* Populate Vendor models */
        baked_ven_model_arrays[plat_idx].resize(ven_models.size() * sizeof(MESHX_MODEL));
        for (size_t m_idx = 0; m_idx < ven_models.size(); ++m_idx) {
            auto& m = ven_models[m_idx];
            m->set_parent_element(el.get());

            /* Get pointer to the baked slot for this vendor model */
            MESHX_MODEL* p_baked = (MESHX_MODEL*)&baked_ven_model_arrays[plat_idx][m_idx * sizeof(MESHX_MODEL)];

            /* Create the platform model directly into the baked slot */
            if (m->plat_model_create(p_baked) != MESHX_SUCCESS) {
                MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT, "Failed to create platform model for Vendor model %d in element %d", (int)m_idx, (int)plat_idx);
                continue;
            }

            if (m->get_plat_model() == NULL) {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "get_plat_model() returned NULL for Vendor model %d after creation", (int)m_idx);
                continue;
            }
        }

        /* Finalize platform element structure */
        uint16_t loc = 0;
        uint8_t sig_cnt = (uint8_t)sig_models.size();
        uint8_t ven_cnt = (uint8_t)ven_models.size();
        esp_ble_mesh_model_t* sig_ptr = (sig_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_sig_model_arrays[plat_idx].data()) : nullptr;
        esp_ble_mesh_model_t* ven_ptr = (ven_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_ven_model_arrays[plat_idx].data()) : nullptr;

        MESHX_ELEMENT* e = &p_elements[plat_idx];
        memcpy((void*)&e->location, &loc, sizeof(e->location));
        memcpy((void*)&e->sig_model_count, &sig_cnt, sizeof(e->sig_model_count));
        memcpy((void*)&e->vnd_model_count, &ven_cnt, sizeof(e->vnd_model_count));
        memcpy((void*)&e->sig_models, &sig_ptr, sizeof(e->sig_models));
        memcpy((void*)&e->vnd_models, &ven_ptr, sizeof(e->vnd_models));

        /*
         * CRITICAL: Point the C++ model wrappers to the baked arrays.
         * The BLE Mesh stack will update these structures (e.g., handles, publication state),
         * and our C++ logic must see those updates.
         */
        for (size_t m_idx = 0; m_idx < sig_models.size(); ++m_idx) {
            void* ptr = &baked_sig_model_arrays[plat_idx][m_idx * sizeof(MESHX_MODEL)];
            sig_models[m_idx]->set_plat_model(reinterpret_cast<MESHX_MODEL*>(ptr));
        }
        for (size_t m_idx = 0; m_idx < ven_models.size(); ++m_idx) {
            void* ptr = &baked_ven_model_arrays[plat_idx][m_idx * sizeof(MESHX_MODEL)];
            ven_models[m_idx]->set_plat_model(reinterpret_cast<MESHX_MODEL*>(ptr));
        }

        /* Register in registry for runtime callbacks/lookups */
        el->on_baked(plat_idx);
        meshXElementRegistry::get_instance().register_element(el.get());
    }

    /* 4. Finalize Composition Object */
    baked_comp.cid = cid;
    baked_comp.pid = pid;
    baked_comp.vid = vid;
    baked_comp.element_count = total_elements;
    baked_comp.elements = p_elements;

    baked_comp_ptr = &baked_comp;
    is_baked = true;

    /* Update the device structure with the baked data */
    pdev->elements = p_elements;
    pdev->element_cnt = total_elements;
    pdev->element_idx = total_elements;

    MESHX_LOGI(MODULE_ID_COMMON, "Composition baked successfully. Total elements: %zu", total_elements);

    return MESHX_SUCCESS;
}
