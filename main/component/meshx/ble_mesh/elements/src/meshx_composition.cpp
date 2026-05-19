/**
 * @file meshx_composition.cpp
 * @brief Implementation of MeshX Dynamic Composition Manager.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

extern "C" {
}
#include <meshx_composition.hpp>
#include <meshx_element_factory.hpp>
#include <meshx_element_class.hpp>
#include <meshx_model_class.hpp>
#include <meshx_element_registry.hpp>
#include <meshx_uvp_model.hpp>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations for legacy platform initialization */
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

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT, "C|P|V|E: 0x%04x|0x%04x|0x%04x|%zu",
               cid, pid, vid, elements.size());
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "MESHX_MODEL size: %d, pub offset: %d", (int)sizeof(MESHX_MODEL), (int)offsetof(MESHX_MODEL, pub));
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "MESHX_MODEL_PUB size: %d", (int)sizeof(MESHX_MODEL_PUB));

    /* Compile-time check for structural alignment */
    static_assert(sizeof(MESHX_MODEL) >= 40, "MESHX_MODEL size is too small!");

    size_t total_elements = elements.size();

    /* Allocate and initialize platform structures */
    baked_elements.assign(total_elements * sizeof(MESHX_ELEMENT), 0);
    baked_sig_model_arrays.resize(total_elements);
    baked_ven_model_arrays.resize(total_elements);

    MESHX_ELEMENT* p_elements = reinterpret_cast<MESHX_ELEMENT*>(baked_elements.data());

    /* Bake all Elements (Index 0 to N) */
    for (size_t i = 0; i < elements.size(); ++i) {
        size_t plat_idx = i;
        auto& el = elements[i];

        /* Clear and refresh model lists to prevent duplicates if bake is re-called */
        el->get_sig_models().clear();
        el->get_ven_models().clear();
        el->list_sig_models();
        el->list_ven_models();

        auto& sig_models = el->get_sig_models();
        auto& ven_models = el->get_ven_models();

        /* Inject UVP Vendor Model (Mandatory for all MeshX elements) */
        bool uvp_present = false;
        for (auto& m : ven_models) {
            if (m->get_model_id() == MESHX_MODEL_ID_UVP) {
                uvp_present = true;
                break;
            }
        }
        if (!uvp_present) {
            ven_models.push_back(std::make_unique<meshXUVPModel>(el.get()));
        }

        /* 1. Pre-calculate counts after filtering */
        size_t actual_sig_cnt = 0;
        for (auto& m : sig_models) {
            if (m->get_model_id() == MESHX_MODEL_ID_CONFIG_SRV && plat_idx > 0) continue;
            actual_sig_cnt++;
        }
        size_t actual_ven_cnt = ven_models.size();

        /* 2. Resize and Zero-Initialize model arrays ONCE to ensure pointer stability */
        baked_sig_model_arrays[plat_idx].assign(actual_sig_cnt * sizeof(MESHX_MODEL), 0);
        baked_ven_model_arrays[plat_idx].assign(actual_ven_cnt * sizeof(MESHX_MODEL), 0);

        MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "Baking Element %d: SIG models: %zu, Vendor models: %zu (Model Size: %zu)",
                   (int)plat_idx, actual_sig_cnt, actual_ven_cnt, sizeof(MESHX_MODEL));

        /* 3. Populate SIG models */
        size_t baked_sig_idx = 0;
        for (auto& m : sig_models) {
            m->set_parent_element(el.get());

            /* Skip Config Server if not primary element to satisfy ESP-IDF requirements */
            if (m->get_model_id() == MESHX_MODEL_ID_CONFIG_SRV && plat_idx > 0) {
                MESHX_LOGW(MODULE_ID_COMMON, "  Skipping Config Server (0x0000) on non-primary element %d", (int)plat_idx);
                continue;
            }

            /* Get pointer to the baked slot for this model */
            MESHX_MODEL* p_baked_base = reinterpret_cast<MESHX_MODEL*>(baked_sig_model_arrays[plat_idx].data());
            MESHX_MODEL* p_baked = p_baked_base + baked_sig_idx;

            /* Explicitly zero the slot again before calling platform create to prevent garbage */
            memset(static_cast<void*>(p_baked), 0, sizeof(MESHX_MODEL));

            /* Create the platform model directly into the baked slot */
            meshx_err_t m_err = m->plat_model_create(p_baked);
            if (m_err != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "Failed to create platform model for SIG model 0x%04x in element %d (err: %d)",
                           m->get_model_id(), (int)plat_idx, m_err);
                continue;
            }

            /* Link the C++ object to the platform struct */
            m->set_plat_model(p_baked);

            // Debug: Verify the baked model state and check for garbage pointers
            MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "  Model %zu ID: 0x%04x, Pub: 0x%04x, Slot Addr: 0x%04x",
                       baked_sig_idx, p_baked->model_id, p_baked->pub, (void*)p_baked);

            if (p_baked->pub) {
                uint32_t pub_ptr_val = (uint32_t)p_baked->pub;
                if (pub_ptr_val < 0x3f000000 || pub_ptr_val > 0x3fffffff) {
                    MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "  CRITICAL: Garbage Pub pointer detected: %p at %p", (void*)p_baked->pub, (void*)&p_baked->pub);
                }
            }

            baked_sig_idx++;
        }

        /* 4. Populate Vendor models */
        size_t baked_ven_idx = 0;
        for (auto& m : ven_models) {
            m->set_parent_element(el.get());

            /* Get pointer to the baked slot for this vendor model */
            MESHX_MODEL* p_baked = reinterpret_cast<MESHX_MODEL*>(baked_ven_model_arrays[plat_idx].data()) + baked_ven_idx;
            memset(static_cast<void*>(p_baked), 0, sizeof(MESHX_MODEL));

            /* Create the platform model directly into the baked slot */
            meshx_err_t m_err = m->plat_model_create(p_baked);
            if (m_err != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "Failed to create platform model for Vendor model 0x%04x in element %d (err: %d)",
                           m->get_model_id(), (int)plat_idx, m_err);
                continue;
            }

            m->set_plat_model(p_baked);
            MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "Baked VND Model 0x%04x (CID 0x%04x) at 0x%x, pub: 0x%x",
                       p_baked->vnd.model_id, p_baked->vnd.company_id, (void*)p_baked, (void*)p_baked->pub);
            baked_ven_idx++;
        }

        /* 5. Finalize platform element structure */
        uint16_t loc = 0;
        uint8_t sig_cnt = (uint8_t)actual_sig_cnt;
        uint8_t ven_cnt = (uint8_t)baked_ven_idx;
        esp_ble_mesh_model_t* sig_ptr = (sig_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_sig_model_arrays[plat_idx].data()) : nullptr;
        esp_ble_mesh_model_t* ven_ptr = (ven_cnt > 0) ? reinterpret_cast<esp_ble_mesh_model_t*>(baked_ven_model_arrays[plat_idx].data()) : nullptr;

        MESHX_ELEMENT* e = &p_elements[plat_idx];
        memcpy((void*)&e->location, &loc, sizeof(e->location));
        memcpy((void*)&e->sig_model_count, &sig_cnt, sizeof(e->sig_model_count));
        memcpy((void*)&e->vnd_model_count, &ven_cnt, sizeof(e->vnd_model_count));
        memcpy((void*)&e->sig_models, &sig_ptr, sizeof(e->sig_models));
        memcpy((void*)&e->vnd_models, &ven_ptr, sizeof(e->vnd_models));

        /* Register in registry for runtime callbacks/lookups */
        el->on_baked(plat_idx);
        meshXElementRegistry::get_instance().register_element(el.get());
    }

    /* 4. Finalize Composition Object */
    baked_comp.cid = cid;
    baked_comp.pid = pid;
    baked_comp.vid = vid;
    baked_comp.element_count = (uint16_t)total_elements;
    baked_comp.elements = p_elements;

    baked_comp_ptr = &baked_comp;
    is_baked = true;

    /* Update the device structure with the baked data */
    pdev->elements = p_elements;
    pdev->element_cnt = (uint16_t)total_elements;
    pdev->element_idx = (uint16_t)total_elements;
    pdev->composition = baked_comp_ptr;

    return MESHX_SUCCESS;

}
