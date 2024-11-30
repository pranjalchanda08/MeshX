/* board.c - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "board.h"

#define TAG "BOARD"

void hw_init(uint8_t element_idx)
{
#if 0
    gpio_reset_pin(element_gpio_list[element_idx]->pin);
    gpio_set_direction(element_gpio_list[element_idx]->pin, element_gpio_list[element_idx]->dir);
    gpio_set_level(element_gpio_list[element_idx]->pin, 0);
#endif 
    ESP_LOGI(TAG, "Element idx[%d] initialised", element_idx);
    return;
}

void hw_state_set(uint8_t element_idx, uint8_t onoff)
{
#if 0
    gpio_set_level(element_gpio_list[element_idx]->pin, onoff);
    ESP_LOGI(TAG, "SRV HW State changed: <%d>, <%d>, <%d>", element_idx, element_gpio_list[element_idx]->pin, onoff);
#endif
    return;
}