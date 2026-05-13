/**
 * @file gpio_test_registry.c
 * @brief Registry for all GPIO-related unit and property tests
 */

#include "meshx_err.h"
#include "gpio_unit_test.h"
#include "gpio_property_test.h"
#include "gpio_integration_test.h"
#include "pwm_property_test.h"
#include "gpio_platform_property_test.h"
#include "gpio_integration_property_test.h"

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Register all GPIO subsystem tests
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t register_all_gpio_tests(void)
{
    meshx_err_t err;

    err = register_gpio_unit_tests();
    if (err != MESHX_SUCCESS) return err;
    err = register_gpio_property_tests();
    if (err != MESHX_SUCCESS) return err;

    err = register_gpio_integration_tests();
    if (err != MESHX_SUCCESS) return err;

    err = register_pwm_property_tests();
    if (err != MESHX_SUCCESS) return err;

    err = register_gpio_platform_property_tests();
    if (err != MESHX_SUCCESS) return err;

    err = register_gpio_integration_property_tests();
    if (err != MESHX_SUCCESS) return err;

    return MESHX_SUCCESS;
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
