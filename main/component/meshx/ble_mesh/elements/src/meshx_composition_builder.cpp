/**
 * @file meshx_composition_builder.cpp
 * @brief Implementation of MeshX Composition Builder and its C-wrapper.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <meshx_composition_builder.hpp>
#include <meshx_composition.hpp>
#include <meshx_device.hpp>
#include <variants/meshx_root_element.hpp>
#include <variants/meshx_uvp_element.hpp>
#include <meshx_element_registry.hpp>
#include <meshx_element_class.hpp>
#include <meshx_c_header.h>

/**
 * @brief Fluent API Implementation
 */
/**
 * @brief Start building the composition
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::begin() {
    auto& comp = meshXComposition::get_instance();
    comp.clear_elements();

    // All compositions must start with a Root Element at Index 0
    comp.get_elements().push_back(std::make_unique<meshXRootElement>());

    return *this;
}

/**
 * @brief Add a Relay Server element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_relay_server() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_RELAY_SERVER));
    return *this;
}

/**
 * @brief Add a Relay Client element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_relay_client() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_RELAY_CLIENT));
    comp.activate_txcm();
    return *this;
}

/**
 * @brief Add a Light CWWW Server element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_cwww_server() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER));
    return *this;
}

/**
 * @brief Add a Light CWWW Client element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_cwww_client() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT));
    comp.activate_txcm();
    return *this;
}

/**
 * @brief Add a Sensor Server element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_sensor_server() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_SENSOR_SERVER));
    return *this;
}

/**
 * @brief Add a Sensor Client element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_sensor_client() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_SENSOR_CLIENT));
    comp.activate_txcm();
    return *this;
}

/**
 * @brief Add an RGB (HSL) Server element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_rgb_server() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER));
    return *this;
}

/**
 * @brief Add an RGB (HSL) Client element
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::add_rgb_client() {
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size());
    comp.get_elements().push_back(std::make_unique<meshXUVPElement>(next_idx, MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT));
    comp.activate_txcm();
    return *this;
}

/**
 * @brief Commit the composition (Triggers Baking)
 * @return Reference to the builder
 */
meshXCompositionBuilder& meshXCompositionBuilder::commit() {
    /* Currently just a marker, bake() happens during meshx_init */
    return *this;
}


/**
 * @brief C-Wrapper Implementation
 */
extern "C" {

/**
 * @brief Restores the NVS context for all elements in the active composition.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_restore_all_element_ctx(void)
{
    meshx_err_t first_err = MESHX_SUCCESS;
    meshx_err_t ctx_err = MESHX_SUCCESS;
    auto all_elements = meshXElementRegistry::get_instance().get_all_elements();

    for (auto const& [idx, el] : all_elements) {
        if (el) {
            ctx_err = el->restore_nvs_context();
            if (ctx_err != MESHX_SUCCESS) {
                MESHX_LOGW(MODULE_ID_COMMON, "Element [%d] NVS ctx restore: 0x%x", idx, ctx_err);
                if (first_err == MESHX_SUCCESS) {
                    first_err = ctx_err;
                }
            }
        }
    }
    return first_err;
}


/**
 * @brief Checks if a dynamic composition has been built.
 * @return true if elements have been added via the builder, false otherwise.
 */
bool meshx_builder_is_active(void) {
    return meshXComposition::get_instance().has_elements();
}

/**
 * @brief Checks if TXCM should be enabled based on the composition.
 * @return true if any client elements were added, false otherwise.
 */
bool meshx_builder_is_txcm_active(void) {
    return meshXComposition::get_instance().is_txcm_active();
}

/**
 * @brief Bakes the dynamic composition into the device structure.
 * @param pdev Pointer to the device structure.
 * @param cid Company ID.
 * @param pid Product ID.
 * @param vid Version ID.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_builder_bake(dev_struct_t *pdev, uint16_t cid, uint16_t pid, uint16_t vid) {
    meshx_err_t err = MESHX_SUCCESS;

    // 1. Initialize the C++ Device wrapper
    meshXDevice::get_instance().init(pdev);

    // 2. Perform the bake
    meshXComposition::get_instance().set_device_struct(pdev);
    err = meshXComposition::get_instance().bake(cid, pid, vid);

    // 3. Visualize the result
    if (err == MESHX_SUCCESS) {
        meshXDevice::get_instance().visualize_status();
    }

    return err;
}

/**
 * @brief Add elements to the composition using the builder.
 * This is a C-friendly wrapper that can be called from C code to add elements before baking.
 *
 * @param type Element type (meshx_element_type_t)
 * @param count Number of elements of this type to add
 *
 * Note: This function can be called multiple times before commit() to add different types and counts of elements.
 * Example usage:
 *  meshx_builder_add_element(MESHX_ELEMENT_TYPE_RELAY_SERVER, 2);
 *  meshx_builder_add_element(MESHX_ELEMENT_TYPE_SENSOR_SERVER, 1);
 *  meshx_builder_commit();
 *
 */
void meshx_builder_add_element(meshx_element_type_t type, uint16_t count) {
    meshXCompositionBuilder builder;
    uint16_t i = 0;

    switch(type) {
        case MESHX_ELEMENT_TYPE_RELAY_SERVER:
            for(i=0; i<count; ++i) builder.add_relay_server();
            break;
        case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
            for(i=0; i<count; ++i) builder.add_relay_client();
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
            for(i=0; i<count; ++i) builder.add_cwww_server();
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
            for(i=0; i<count; ++i) builder.add_cwww_client();
            break;
        case MESHX_ELEMENT_TYPE_SENSOR_SERVER:
            for(i=0; i<count; ++i) builder.add_sensor_server();
            break;
        case MESHX_ELEMENT_TYPE_SENSOR_CLIENT:
            for(i=0; i<count; ++i) builder.add_sensor_client();
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER:
            for(i=0; i<count; ++i) builder.add_rgb_server();
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT:
            for(i=0; i<count; ++i) builder.add_rgb_client();
            break;
        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Builder: No implementation for element type %d", type);
            break;
    }
}

/**
 * @brief C-Friendly Builder functions
 */

/**
 * @brief Resets and starts a new dynamic composition build session.
 */
void meshx_builder_begin(void) {
    meshXCompositionBuilder builder;
    builder.begin();
}

/**
 * @brief Clears all elements from the active composition builder session.
 */
void meshx_builder_clear(void) {
    meshXComposition::get_instance().clear_elements();
}

/**
 * @brief Commits the pending dynamic composition build session.
 */
void meshx_builder_commit(void) {
    meshXCompositionBuilder builder;
    builder.commit();
}

/**
 * @brief Saves the current dynamic composition schema to NVS.
 *
 * Iterates through active elements in the composition (skipping root at index 0)
 * and writes the element type array into NVS.
 *
 * @param[in] override_ro Set to true if this schema should take precedence over factory RO configuration.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully written to NVS.
 *     - MESHX_INVALID_STATE: Composition is empty.
 *     - Error code from meshx_nvs_comp_schema_set on write failure.
 */
meshx_err_t meshx_builder_save_to_nvs(bool override_ro) {
    meshXComposition& comp = meshXComposition::get_instance();
    auto& elements = comp.get_elements();
    meshx_nvs_comp_schema_t schema;
    uint8_t count = 0;
    size_t i = 1;

    if (elements.empty()) return MESHX_INVALID_STATE;

    memset(&schema, 0, sizeof(schema));
    schema.magic = MESHX_NVS_COMP_MAGIC;
    schema.version = MESHX_NVS_COMP_VERSION;
    schema.flags = override_ro ? MESHX_COMP_FLAG_OVERRIDE_RO : 0;

    // Element 0 is always Root Element; save all dynamic elements from index 1 upwards
    for (i = 1; i < elements.size() && count < CONFIG_MAX_ELEMENT_COUNT; ++i) {
        if (elements[i]) {
            schema.element_types[count++] = (uint16_t)elements[i]->get_element_variant();
        }
    }
    schema.element_count = count;

    return meshx_nvs_comp_schema_set(&schema);
}

/**
 * @brief Get element composition data for all elements in the composition
 * This is used to sync the composition structure with the host during initialization.
 *
 * @param[out] buf     Buffer to write the composition data into.
 * @param[in]  max_len Maximum length of the buffer.
 * @return Number of bytes written to the buffer.
 */
size_t meshx_get_element_composition_data(uint8_t *buf, size_t max_len) {
    meshXComposition& comp = meshXComposition::get_instance();
    auto& elements = comp.get_elements();
    size_t offset = 1;
    const char* name = NULL;
    size_t name_len = 0;
    mxcp_comp_entry_header_t entry;

    if (!buf || max_len < 2) return 0;

    buf[0] = (uint8_t)elements.size();

    for (const auto& el : elements) {
        if (!el) continue;

        name = el->get_element_name();
        name_len = strlen(name) + 1;
        if (offset + sizeof(mxcp_comp_entry_header_t) + name_len > max_len) {
            break;
        }

        entry.idx = el->get_element_idx();
        entry.variant = (uint16_t)el->get_element_variant();
        entry.type = (uint16_t)el->get_element_type();

        memcpy(&buf[offset], &entry, sizeof(entry));
        offset += sizeof(entry);
        memcpy(&buf[offset], name, name_len);
        offset += name_len;
    }

    return offset;
}

/**
 * @brief Get element state data for all elements in the composition
 * This is used to sync the current state of the composition with the host during initialization.
 *
 * @param[out] buf     Buffer to write the state data into.
 * @param[in]  max_len Maximum length of the buffer.
 * @return Number of bytes written to the buffer.
 */
size_t meshx_get_element_state_data(uint8_t *buf, size_t max_len) {
    meshXComposition& comp = meshXComposition::get_instance();
    auto& elements = comp.get_elements();
    size_t offset = 1;
    size_t ctx_size = 0;
    size_t telemetry_size = 0;
    size_t len = 0;
    const uint8_t* state = NULL;
    mxcp_state_entry_header_t entry;

    if (!buf || max_len < 2) return 0;

    buf[0] = (uint8_t)elements.size();

    for (const auto& el : elements) {
        if (!el) continue;

        ctx_size = el->get_element_ctx_size();
        telemetry_size = 0;
        
        // Compute total telemetry size for this element
        auto logical_models = el->get_logical_models_ptr();
        if (logical_models) {
            for (const auto& m : *logical_models) {
                if (m) {
                    m->get_cached_state(len);
                    telemetry_size += len;
                }
            }
        }

        if (offset + sizeof(mxcp_state_entry_header_t) + ctx_size + telemetry_size > max_len) {
            break;
        }

        entry.idx = el->get_element_idx();
        entry.variant = (uint16_t)el->get_element_variant();
        entry.ctx_size = (uint16_t)ctx_size;
        entry.telemetry_size = (uint16_t)telemetry_size;

        memcpy(&buf[offset], &entry, sizeof(entry));
        offset += sizeof(entry);

        if (ctx_size > 0 && el->get_element_ctx()) {
            memcpy(&buf[offset], el->get_element_ctx(), ctx_size);
            offset += ctx_size;
        }
        
        // Append telemetry state data
        if (telemetry_size > 0 && logical_models) {
            for (const auto& m : *logical_models) {
                if (m) {
                    state = m->get_cached_state(len);
                    if (state && len > 0) {
                        memcpy(&buf[offset], state, len);
                        offset += len;
                    }
                }
            }
        }
    }

    return offset;
}

/**
 * @brief Get the number of elements in the active composition.
 * @return Total number of elements.
 */
uint8_t meshx_comp_get_element_count(void) {
    meshXComposition& comp = meshXComposition::get_instance();
    return (uint8_t)comp.get_elements().size();
}

/**
 * @brief Get the variant of a specific element in the active composition.
 * @param element_idx Index of the element.
 * @return Element variant, or 0 if invalid.
 */
uint16_t meshx_comp_get_element_variant(uint8_t element_idx) {
    meshXComposition& comp = meshXComposition::get_instance();
    const auto& elements = comp.get_elements();
    if (element_idx < elements.size() && elements[element_idx]) {
        return (uint16_t)elements[element_idx]->get_element_variant();
    }
    return 0;
}

/**
 * @brief Get the list of functional IDs supported by an element.
 * @param element_idx Index of the element.
 * @param out_func_ids Destination buffer to receive function IDs.
 * @param max_count Maximum number of function IDs to store.
 * @return Number of function IDs written.
 */
uint8_t meshx_comp_get_element_func_ids(uint8_t element_idx, uint16_t *out_func_ids, uint8_t max_count) {
    if (!out_func_ids || max_count == 0) return 0;
    meshXComposition& comp = meshXComposition::get_instance();
    const auto& elements = comp.get_elements();
    if (element_idx >= elements.size() || !elements[element_idx]) return 0;

    auto models = elements[element_idx]->get_logical_models_ptr();
    if (!models) return 0;

    uint8_t count = 0;
    for (const auto& m : *models) {
        if (m && count < max_count) {
            out_func_ids[count++] = m->get_func_id();
        }
    }
    return count;
}

/**
 * @brief Check if a specific element contains a logical model with the given func_id.
 * @param element_idx Index of the element.
 * @param func_id Functional ID to query.
 * @return true if supported, false otherwise.
 */
bool meshx_comp_element_has_func_id(uint8_t element_idx, uint16_t func_id) {
    meshXComposition& comp = meshXComposition::get_instance();
    const auto& elements = comp.get_elements();
    if (element_idx >= elements.size() || !elements[element_idx]) return false;

    auto models = elements[element_idx]->get_logical_models_ptr();
    if (!models) return false;

    for (const auto& m : *models) {
        if (m && m->get_func_id() == func_id) {
            return true;
        }
    }
    return false;
}

} /* extern "C" */
