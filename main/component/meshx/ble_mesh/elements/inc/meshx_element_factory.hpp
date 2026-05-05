/**
 * @file meshx_element_factory.hpp
 * @brief Template-based factory helper for creating MeshX elements.
 * 
 * This header provides a generic factory function to reduce boilerplate code 
 * when creating and registering different types of mesh elements.
 * 
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_ELEMENT_FACTORY_HPP__
#define __MESHX_ELEMENT_FACTORY_HPP__

#include <meshx_element_class.hpp>
#include <meshx_nvs.h>
#include <meshx_api.h>
#include <new>

/**
 * @brief Helper function to create and register mesh elements.
 * 
 * @tparam ElementType The concrete class of the element to create.
 * @tparam ContextType The type of the element's NVS context structure.
 * 
 * @param pdev         Pointer to the device structure.
 * @param requested_cnt Number of elements requested to be created.
 * @param max_cnt      Compile-time maximum number of elements supported.
 * @param module_id    Module identifier for logging.
 * @param log_name     Human-readable name of the element type for logging.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
template <typename ElementType, typename ContextType>
meshx_err_t meshx_element_factory_helper(
    dev_struct_t *pdev,
    uint16_t requested_cnt,
    uint16_t max_cnt,
    module_id_t module_id,
    const char *log_name)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (requested_cnt > max_cnt)
    {
        MESHX_LOGW(module_id,
                   "%s: requested count %d exceeds compile-time max %d. Capping.",
                   log_name, requested_cnt, max_cnt);
        requested_cnt = max_cnt;
    }

    for (uint16_t i = 0; i < requested_cnt; i++)
    {
        uint16_t abs_idx = (uint16_t)(pdev->element_idx);

        /* Construct element */
        auto *el = new (std::nothrow) ElementType(abs_idx);
        if (!el)
        {
            MESHX_LOGE(module_id, "%s [%d]: allocation failed", log_name, abs_idx);
            return MESHX_NO_MEM;
        }

        /* Initialize element (lists models, allocates platform arrays, adds models) */
        err = el->initialize();
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(module_id, "%s [%d]: initialization failed: 0x%x", log_name, abs_idx, err);
            return err;
        }

        /* Restore NVS context if available (initialize already calls this, but we can keep it explicit if preferred or rely on initialize) */
        /* For consistency, we rely on initialize() to handle the full setup including NVS restore */

        /* Add element to composition */
        MESHX_LOGI(module_id, "%s [%d]: Registering Element with SIG=%d, VEN=%d models", 
                   log_name, abs_idx, el->get_no_of_sig_models(), el->get_no_of_ven_models());

        err = meshx_plat_add_element_to_composition(
            abs_idx,
            pdev->elements,
            el->get_sig_plat_model_array(),
            el->get_ven_plat_model_array(),
            el->get_no_of_sig_models(),
            el->get_no_of_ven_models());

        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(module_id, "%s [%d]: composition registration failed: 0x%x",
                       log_name, abs_idx, err);
            return err;
        }

        /* Increment device element index */
        pdev->element_idx++;
    }

    return MESHX_SUCCESS;
}

#endif /* __MESHX_ELEMENT_FACTORY_HPP__ */
