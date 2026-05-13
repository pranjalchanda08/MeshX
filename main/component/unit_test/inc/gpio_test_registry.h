/**
 * @file gpio_test_registry.h
 * @brief Header for GPIO test registry
 */

#ifndef __GPIO_TEST_REGISTRY_H
#define __GPIO_TEST_REGISTRY_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register all GPIO subsystem tests
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t register_all_gpio_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_TEST_REGISTRY_H */
