#ifndef __MESHX_COMPOSITION_HPP__
#define __MESHX_COMPOSITION_HPP__

#include <meshx_fwd_decl.hpp>
#include <vector>
#include <memory>

/**
 * @class meshXComposition
 * @brief Singleton manager for the dynamic BLE Mesh composition.
 * @details This class manages the collection of elements and flattens them into
 *          the contiguous C-arrays required by the platform's BLE Mesh stack.
 */
class meshXComposition {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the meshXComposition instance
     */
    static meshXComposition& get_instance();

    /**
     * @brief Add an element to the composition
     * @param element Unique pointer to the element to add
     */
    void add_element(std::unique_ptr<meshXElementIF> element) {
        elements.push_back(std::move(element));
    }

    /**
     * @brief Get the list of elements
     */
    std::vector<std::unique_ptr<meshXElementIF>>& get_elements() { return elements; }

    /**
     * @brief Check if any element in the composition contains a specific model.
     * @param model_id The model ID to search for.
     * @return true if found, false otherwise.
     */
    bool has_model(uint16_t model_id) const {
        for (const auto& el : elements) {
            if (el && el->has_model(model_id)) return true;
        }
        return false;
    }

    /**
     * @brief Check if there are any elements
     */
    bool has_elements() const { return !elements.empty(); }

    /**
     * @brief Clear all elements and reset state
     */
    void clear_elements() {
        elements.clear();
        is_baked = false;
        baked_elements.clear();
        baked_sig_model_arrays.clear();
        baked_ven_model_arrays.clear();
    }

    /**
     * @brief Set the device structure pointer
     * @param pdev Pointer to the device structure
     */
    void set_device_struct(dev_struct_t *pdev) { this->pdev = pdev; }

    /**
     * @brief "Bake" the dynamic composition into a contiguous C-array
     * @details This flattens the C++ object tree into the format required by the BLE stack.
     * @param cid Company ID.
     * @param pid Product ID.
     * @param vid Version ID.
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    meshx_err_t bake(uint16_t cid, uint16_t pid, uint16_t vid);

    /**
     * @brief Check if the composition is baked
     * @return true if baked, false otherwise
     */
    bool is_composition_baked() const { return is_baked; }

    /**
     * @brief Get the baked elements array
     */
    MESHX_ELEMENT* get_baked_elements() {
        return reinterpret_cast<MESHX_ELEMENT*>(baked_elements.data());
    }

    /**
     * @brief Get the baked composition structure
     */
    MESHX_COMPOSITION* get_baked_composition() { return baked_comp_ptr; }

private:
    meshXComposition() : pdev(nullptr), is_baked(false), baked_comp_ptr(nullptr) {}
    ~meshXComposition() = default;

    // Delete copy/move to enforce singleton
    meshXComposition(const meshXComposition&) = delete;
    meshXComposition& operator=(const meshXComposition&) = delete;

    dev_struct_t *pdev;
    bool is_baked;

    // C++ object collection
    std::vector<std::unique_ptr<meshXElementIF>> elements;

    // Dynamic storage using byte vectors to bypass const-member assignment issues
    std::vector<uint8_t> baked_elements;

    // Contiguous model arrays for each element
    // Index 0 is for root element, 1+ for dynamic elements
    // Using vector<uint8_t> to avoid C++ constructor/copy issues with platform structs
    std::vector<std::vector<uint8_t>> baked_sig_model_arrays;
    std::vector<std::vector<uint8_t>> baked_ven_model_arrays;

    MESHX_COMPOSITION baked_comp;
    MESHX_COMPOSITION* baked_comp_ptr;
};

#endif /* __MESHX_COMPOSITION_HPP__ */
