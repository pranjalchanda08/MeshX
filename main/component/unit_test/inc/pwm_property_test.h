/**
 * @file pwm_property_test.h
 * @brief Header for PWM property-based tests
 *
 * This file declares the PWM property test functions and registration.
 *
 * **Validates: Requirements 4.1-4.10**
 *
 * Property 4: PWM Subsystem Correctness
 * For any GPIO pin configured for PWM output (with frequency, duty cycle, resolution, and channel specifications), the PWM subsystem SHALL:
 * 1. Initialize based on YAML configuration
 * 2. Start and stop PWM output as requested
 * 3. Set and get duty cycle (0-100%) and frequency values accurately
 * 4. Validate parameters against hardware limits
 * 5. Handle hardware channel allocation and conflicts
 * 6. Maintain PWM state for runtime control and monitoring
 * 7. Deinitialize and free resources during shutdown
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __PWM_PROPERTY_TEST_H
#define __PWM_PROPERTY_TEST_H

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register PWM property tests with unit test framework
 *
 * @return meshx_err_t Registration result
 */
meshx_err_t register_pwm_property_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __PWM_PROPERTY_TEST_H */
