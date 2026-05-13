/**
 * @file gpio_integration_property_test.h
 * @brief Header for GPIO integration property-based tests
 */

#ifndef __GPIO_INTEGRATION_PROPERTY_TEST_H
#define __GPIO_INTEGRATION_PROPERTY_TEST_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register GPIO integration property tests
 * @return meshx_err_t Registration result
 */
meshx_err_t register_gpio_integration_property_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_INTEGRATION_PROPERTY_TEST_H */
