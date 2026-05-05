/**
 * @file meshx_device.cpp
 * @brief Implementation of the meshXDevice wrapper.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <meshx_device.hpp>
#include <meshx_element_registry.hpp>
#include <meshx_element_class.hpp>
#include <meshx_model_class.hpp>

meshXDevice& meshXDevice::get_instance() {
    static meshXDevice instance;
    return instance;
}

void meshXDevice::visualize_status() {
    if (!pdev) {
        MESHX_LOGW(MODULE_ID_COMMON, "Device not initialized, cannot visualize status");
        return;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "==================================================");
    MESHX_LOGI(MODULE_ID_COMMON, "   MESHX DEVICE STATUS VISUALIZATION");
    MESHX_LOGI(MODULE_ID_COMMON, "==================================================");
    MESHX_LOGI(MODULE_ID_COMMON, "Device UUID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               pdev->uuid[0], pdev->uuid[1], pdev->uuid[2], pdev->uuid[3],
               pdev->uuid[4], pdev->uuid[5], pdev->uuid[6], pdev->uuid[7],
               pdev->uuid[8], pdev->uuid[9], pdev->uuid[10], pdev->uuid[11],
               pdev->uuid[12], pdev->uuid[13], pdev->uuid[14], pdev->uuid[15]);
    MESHX_LOGI(MODULE_ID_COMMON, "Total Elements: %zu", pdev->element_cnt);
    MESHX_LOGI(MODULE_ID_COMMON, "--------------------------------------------------");

    auto& registry = meshXElementRegistry::get_instance();
    
    for (size_t i = 0; i < pdev->element_cnt; ++i) {
        meshXElementIF* el = registry.find_element((uint16_t)i);
        
        if (el) {
            MESHX_LOGI(MODULE_ID_COMMON, "Element [%zu]: (Addr: %p)", i, (void*)el);
            
            // List SIG Models
            auto& sig_models = el->get_sig_models();
            for (auto& m : sig_models) {
                MESHX_LOGI(MODULE_ID_COMMON, "  - SIG Model: 0x%04x (%s)", 
                           m->get_model_id(), m->is_initialized() ? "Active" : "Idle");
            }
            
            // List Vendor Models
            auto& ven_models = el->get_ven_models();
            for (auto& m : ven_models) {
                MESHX_LOGI(MODULE_ID_COMMON, "  - VND Model: 0x%04x", m->get_model_id());
            }
        } else {
            MESHX_LOGW(MODULE_ID_COMMON, "Element [%zu]: NO C++ OBJECT MAPPED", i);
        }
    }
    MESHX_LOGI(MODULE_ID_COMMON, "==================================================");
}
