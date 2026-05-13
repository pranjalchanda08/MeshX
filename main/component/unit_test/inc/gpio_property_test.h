/**
 * @file gpio_property_test.h
 * @brief Header for GPIO property-based tests
 *
 * This file declares the GPIO property test functions and registration.
 *
 * **Validates: Requirements 2.1-2.5, 2.8-2.10, 8.1-8.3, 8.9-8.10**
 *
 * Property 2: Mode-Aware GPIO Operation Validity
 * For any GPIO pin configured in a specific mode (input, output, open-drain,
 * input/output, PWM output), operations attempted on that pin SHALL:
 * 1. Succeed only if valid for the pin's configured mode
 * 2. Return appropriate standardized error codes for invalid operations
 * 3. Not affect the state of other GPIO pins
 * 4. Maintain consistent pin state tracking for runtime validation
 * 5. Initialize and deinitialize pins correctly during system startup/shutdown
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __GPIO_PROPERTY_TEST_H
#define __GPIO_PROPERTY_TEST_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register GPIO property tests with unit test framework
 *
 * @return meshx_err_t Registration result
 */
meshx_err_t register_gpio_property_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_PROPERTY_TEST_H */
