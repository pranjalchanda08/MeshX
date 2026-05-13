/**
 * @file meshx_io_bridge.cpp
 * @brief MeshX IO Bridge Implementation
 *
 * This file implements the C bridge for the C++ IO abstraction layer.
 * It provides the glue between C applications and C++ IO instances.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "../interface/meshx_io_bridge.h"
#include "../inc/meshx_io_factory.hpp"
#include "../inc/meshx_io_interface.hpp"
#include "../inc/meshx_io_types.hpp"
#include <vector>
#include <memory>

using namespace meshx;

extern "C" {

meshx_io_handle_t meshx_io_create(const meshx_io_config_t* config) {
    if (config == nullptr) {
        return nullptr;
    }

    IoConfig io_config;
    io_config.logical_pin = config->logical_pin;
    io_config.type = static_cast<IoType>(config->io_type);
    io_config.name = config->name ? config->name : "";

    // Copy configuration based on type
    if (io_config.type == IoType::GPIO) {
        io_config.config.gpio.mode = config->config.gpio.mode;
        io_config.config.gpio.pull = config->config.gpio.pull;
        io_config.config.gpio.drive = config->config.gpio.drive;
        io_config.config.gpio.initial_level = config->config.gpio.initial_level;
        io_config.config.gpio.signal_inversion = config->config.gpio.signal_inversion;
    } else if (io_config.type == IoType::PWM) {
        io_config.config.pwm.frequency = config->config.pwm.frequency;
        io_config.config.pwm.duty_cycle = config->config.pwm.duty_cycle;
        io_config.config.pwm.resolution = config->config.pwm.resolution;
        io_config.config.pwm.channel = config->config.pwm.channel;
    }

    auto instance = MeshXIoFactory::getInstance().create(io_config);
    if (!instance) {
        return nullptr;
    }

    // Return as raw pointer (opaque handle)
    return static_cast<meshx_io_handle_t>(instance.release());
}

void meshx_io_destroy(meshx_io_handle_t handle) {
    if (handle != nullptr) {
        delete static_cast<MeshXIoInterface*>(handle);
    }
}

meshx_err_t meshx_io_execute(meshx_io_handle_t handle,
                             meshx_io_func_t function,
                             const uint32_t* args,
                             uint8_t arg_count) {
    if (handle == nullptr) {
        return MESHX_INVALID_ARG;
    }

    MeshXIoInterface* instance = static_cast<MeshXIoInterface*>(handle);
    std::vector<uint32_t> arg_vec;
    if (args != nullptr && arg_count > 0) {
        arg_vec.assign(args, args + arg_count);
    }

    int result = instance->execute(static_cast<IoFunction>(function), arg_vec);
    return (result == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

uint8_t meshx_io_get_logical_pin(meshx_io_handle_t handle) {
    if (handle == nullptr) {
        return 0xFF;
    }
    return static_cast<MeshXIoInterface*>(handle)->getLogicalPin();
}

const char* meshx_io_get_name(meshx_io_handle_t handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return static_cast<MeshXIoInterface*>(handle)->getName();
}

bool meshx_io_is_function_supported(meshx_io_handle_t handle,
                                    meshx_io_func_t function) {
    if (handle == nullptr) {
        return false;
    }
    return static_cast<MeshXIoInterface*>(handle)->isFunctionSupported(static_cast<IoFunction>(function));
}

meshx_err_t meshx_io_set_level(meshx_io_handle_t handle, uint8_t level) {
    if (handle == nullptr) {
        return MESHX_INVALID_ARG;
    }
    int result = static_cast<MeshXIoInterface*>(handle)->setLevel(level);
    return (result == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

meshx_err_t meshx_io_get_level(meshx_io_handle_t handle, uint8_t* level) {
    if (handle == nullptr || level == nullptr) {
        return MESHX_INVALID_ARG;
    }
    int result = static_cast<MeshXIoInterface*>(handle)->getLevel(level);
    return (result == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

meshx_err_t meshx_io_toggle(meshx_io_handle_t handle) {
    if (handle == nullptr) {
        return MESHX_INVALID_ARG;
    }
    int result = static_cast<MeshXIoInterface*>(handle)->toggle();
    return (result == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

meshx_err_t meshx_io_init(void) {
    // Factory initializes itself on getInstance()
    (void)MeshXIoFactory::getInstance();
    return MESHX_SUCCESS;
}

meshx_err_t meshx_io_deinit(void) {
    // No specific deinit needed for factory singleton
    return MESHX_SUCCESS;
}

} // extern "C"
