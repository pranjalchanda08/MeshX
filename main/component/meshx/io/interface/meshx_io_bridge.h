/**
 * @file meshx_io_bridge.h
 * @brief MeshX IO Bridge for C/C++ Boundary
 *
 * This file provides C interface for the C++ IO abstraction layer.
 * It follows MeshX's C/C++ boundary patterns.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_IO_BRIDGE_H
#define __MESHX_IO_BRIDGE_H

#include "meshx_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to IO instance
 */
typedef void* meshx_io_handle_t;

/**
 * @brief IO function types (C compatible)
 */
typedef enum {
    MESHX_IO_FUNC_SET_LEVEL = 0,
    MESHX_IO_FUNC_GET_LEVEL,
    MESHX_IO_FUNC_TOGGLE,
    MESHX_IO_FUNC_SET_PWM_DUTY,
    MESHX_IO_FUNC_SET_PWM_FREQUENCY,
    MESHX_IO_FUNC_REGISTER_INTERRUPT,
    MESHX_IO_FUNC_UNREGISTER_INTERRUPT,
    MESHX_IO_FUNC_ENABLE_INTERRUPT,
    MESHX_IO_FUNC_CUSTOM,
    MESHX_IO_FUNC_MAX
} meshx_io_func_t;

/**
 * @brief IO configuration structure (C compatible)
 */
typedef struct {
    uint8_t logical_pin;
    uint8_t io_type;
    const char* name;

    union {
        struct {
            uint8_t mode;
            uint8_t pull;
            uint8_t drive;
            uint8_t initial_level;
            bool signal_inversion;
        } gpio;

        struct {
            uint32_t frequency;
            uint8_t duty_cycle;
            uint8_t resolution;
            uint8_t channel;
        } pwm;

        struct {
            uint16_t custom_id;
            uint32_t custom_data;
        } custom;
    } config;
} meshx_io_config_t;

/**
 * @brief Create IO instance from configuration
 *
 * @param config IO configuration
 * @return meshx_io_handle_t IO instance handle, NULL on failure
 */
meshx_io_handle_t meshx_io_create(const meshx_io_config_t* config);

/**
 * @brief Destroy IO instance
 *
 * @param handle IO instance handle
 */
void meshx_io_destroy(meshx_io_handle_t handle);

/**
 * @brief Execute IO function
 *
 * @param handle IO instance handle
 * @param function IO function to execute
 * @param args Function arguments array
 * @param arg_count Number of arguments
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_execute(meshx_io_handle_t handle,
                             meshx_io_func_t function,
                             const uint32_t* args,
                             uint8_t arg_count);

/**
 * @brief Get logical pin number
 *
 * @param handle IO instance handle
 * @return uint8_t Logical pin number
 */
uint8_t meshx_io_get_logical_pin(meshx_io_handle_t handle);

/**
 * @brief Get pin name
 *
 * @param handle IO instance handle
 * @return const char* Pin name
 */
const char* meshx_io_get_name(meshx_io_handle_t handle);

/**
 * @brief Check if function is supported
 *
 * @param handle IO instance handle
 * @param function IO function to check
 * @return true if function is supported
 * @return false if function is not supported
 */
bool meshx_io_is_function_supported(meshx_io_handle_t handle,
                                    meshx_io_func_t function);

/**
 * @brief Set pin level (convenience function)
 *
 * @param handle IO instance handle
 * @param level Pin level (0 = low, 1 = high)
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_set_level(meshx_io_handle_t handle, uint8_t level);

/**
 * @brief Get pin level (convenience function)
 *
 * @param handle IO instance handle
 * @param[out] level Pointer to store pin level
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_get_level(meshx_io_handle_t handle, uint8_t* level);

/**
 * @brief Toggle pin level (convenience function)
 *
 * @param handle IO instance handle
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_toggle(meshx_io_handle_t handle);

/**
 * @brief Initialize IO subsystem
 *
 * This function initializes the IO factory and registers all IO types.
 *
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_init(void);

/**
 * @brief Deinitialize IO subsystem
 *
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_IO_BRIDGE_H */
