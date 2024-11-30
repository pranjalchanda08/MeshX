/* board.h - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif /**< __cplusplus */

#include "driver/gpio.h"

#if defined(CONFIG_BLE_MESH_ESP32C3_DEV)
#define LED_R GPIO_NUM_8
#define LED_G GPIO_NUM_8
#define LED_B GPIO_NUM_8
#endif

typedef struct gpio_state {
    uint16_t current;
    uint16_t previous;
}gpio_state_t;

typedef struct gpio_handle {
    gpio_num_t pin;
    uint8_t func;
    uint8_t dir;
    gpio_state_t states;
}gpio_handle_t;

extern gpio_handle_t * element_gpio_list[];
extern uint16_t element_gpio_list_len;

void hw_init(uint8_t element_idx);
void hw_state_set(uint8_t element_idx, uint8_t onoff);

#ifdef __cplusplus
}
#endif /**< __cplusplus */

#endif /* _BOARD_H_ */
