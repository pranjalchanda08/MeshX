/**
 * @file meshx_device.hpp
 * @brief C++ Wrapper for dev_struct_t to provide high-level management and visualization.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_DEVICE_HPP__
#define __MESHX_DEVICE_HPP__

#include <meshx_common.h>
#include <meshx_fwd_decl.hpp>

/**
 * @class meshXDevice
 * @brief Singleton wrapper for the raw C dev_struct_t.
 * @details This class provides a bridge between the C-centric device structure
 *          and the rich C++ element/model system.
 */
class meshXDevice {
public:
    /**
     * @brief Get the singleton instance
     */
    static meshXDevice& get_instance();

    /**
     * @brief Initialize the device wrapper with a raw C struct
     * @param pdev Pointer to the dev_struct_t
     */
    void init(dev_struct_t* pdev) {
        this->pdev = pdev;
    }

    /**
     * @brief Get the underlying C structure
     */
    dev_struct_t* get_raw_struct() { return pdev; }

    /**
     * @brief Check if initialized
     */
    bool is_initialized() const { return pdev != nullptr; }

    /**
     * @brief Visualize the current composition and element status
     */
    void visualize_status();

private:
    meshXDevice() : pdev(nullptr) {}
    ~meshXDevice() = default;

    dev_struct_t* pdev;
};

#endif /* __MESHX_DEVICE_HPP__ */
