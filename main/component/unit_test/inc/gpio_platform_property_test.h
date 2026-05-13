/**
 * @file gpio_platform_property_test.h
 * @brief Header for GPIO platform property-based tests
 */

#ifndef __GPIO_PLATFORM_PROPERTY_TEST_H
#define __GPIO_PLATFORM_PROPERTY_TEST_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register GPIO platform property tests
 * @return meshx_err_t Registration result
 */
meshx_err_t register_gpio_platform_property_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_PLATFORM_PROPERTY_TEST_H */
