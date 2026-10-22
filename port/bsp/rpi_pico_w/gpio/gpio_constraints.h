/**
 * @file gpio_constraints.h
 * @brief GPIO Constraints for Raspberry Pi Pico W BSP (RP2040)
 */

#ifndef __RPI_PICO_W_GPIO_CONSTRAINTS_H
#define __RPI_PICO_W_GPIO_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPI_PICO_W_PWM_MIN_FREQUENCY 10
#define RPI_PICO_W_PWM_MAX_FREQUENCY 62500000
#define RPI_PICO_W_PWM_MIN_RESOLUTION 1
#define RPI_PICO_W_PWM_MAX_RESOLUTION 16
#define RPI_PICO_W_PWM_MAX_CHANNELS 16

#define RPI_PICO_W_INTERRUPT_TASK_STACK_SIZE 2048
#define RPI_PICO_W_INTERRUPT_TASK_PRIORITY 5

typedef struct {
    uint8_t pin;
    const char* conflict;
    const char* recommendation;
} rpi_pico_w_pin_conflict_t;

bool rpi_pico_w_gpio_has_conflict(uint8_t physical_pin);
const char* rpi_pico_w_gpio_get_conflict(uint8_t physical_pin);
const char* rpi_pico_w_gpio_get_recommendation(uint8_t physical_pin);
bool rpi_pico_w_pwm_is_frequency_valid(uint32_t frequency);
bool rpi_pico_w_pwm_is_duty_cycle_valid(uint8_t duty_cycle);
bool rpi_pico_w_pwm_is_resolution_valid(uint8_t resolution);
uint32_t rpi_pico_w_pwm_get_recommended_frequency(uint8_t resolution);
uint8_t rpi_pico_w_pwm_get_available_channels(void);
bool rpi_pico_w_gpio_is_interrupt_supported(uint8_t physical_pin);
uint16_t rpi_pico_w_gpio_get_recommended_interrupt_stack_size(void);
uint8_t rpi_pico_w_gpio_get_recommended_interrupt_priority(void);

#ifdef __cplusplus
}
#endif

#endif /* __RPI_PICO_W_GPIO_CONSTRAINTS_H */
