/**
 * @file gpio_unit_test.h
 * @brief Header for GPIO unit tests
 *
 * This file declares the GPIO unit test functions and registration.
 *
 * **Validates: Requirements 10.1-10.4**
 *
 * These unit tests validate GPIO API functions with:
 * - Valid and invalid inputs
 * - Error conditions and recovery strategies
 * - Function-based API with various argument vectors
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __GPIO_UNIT_TEST_H
#define __GPIO_UNIT_TEST_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register GPIO unit tests with unit test framework
 *
 * @return meshx_err_t Registration result
 */
meshx_err_t register_gpio_unit_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_UNIT_TEST_H */
