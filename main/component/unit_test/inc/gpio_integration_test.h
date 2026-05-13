/**
 * @file gpio_integration_test.h
 * @brief Header for GPIO integration tests
 *
 * This file declares the GPIO integration test registration function
 * for testing GPIO with elements, hosted mode, and KV Engine persistence.
 *
 * **Validates: Requirements 10.5-10.8**
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __GPIO_INTEGRATION_TEST_H__
#define __GPIO_INTEGRATION_TEST_H__

#include "unit_test.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register GPIO integration tests with the unit test framework.
 *
 * This function registers all GPIO integration tests including:
 * - Element integration tests (IO interface, state changes)
 * - Hosted/non-hosted mode switching tests
 * - KV Engine persistence and recovery tests
 *
 * Test command IDs (use cmd_id 100+ to avoid conflict with unit tests):
 * - 100: Run all integration tests
 * - 101-103: Element integration tests
 * - 104-106: Hosted mode tests
 * - 107-113: KV Engine persistence tests
 * - 114-115: Cross-subsystem tests
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t register_gpio_integration_tests(void);

#endif /* CONFIG_ENABLE_UNIT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_INTEGRATION_TEST_H__ */
