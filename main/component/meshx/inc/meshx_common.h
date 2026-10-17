/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_common.h
 * @brief Common application definitions and includes for BLE Mesh Node.
 *
 * This header file contains common definitions, includes, and data structures
 * used across the BLE Mesh Node application.
 *
 * @author Pranjal Chanda
 *
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <meshx_err.h>
#include <meshx_config_internal.h>
#include <interface/logging/meshx_log.h>
#include "interface/meshx_platform.h"
#include "interface/rtos/meshx_rtos_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration for the BLE Mesh application control event.
 */
typedef enum {
    MESHX_CTRL_EVT_NODE_RESET = 0x01,   /* Node Reset */
    MESHX_CTRL_EVT_PROV_COMP,           /* Provisioning Completed */
    MESHX_CTRL_EVT_PROV_FAILED,         /* Provisioning Failed */
    MESHX_CTRL_EVT_PROV_START,          /* Provisioning Started */
    MESHX_CTRL_EVT_IDENTIFY_START,      /* Identification Started */
    MESHX_CTRL_EVT_IDENTIFY_STOP,       /* Identification Stopped */
    MESHX_CTRL_EVT_GET_COMPOSITION = 0x07, /* Get dynamic composition */
    MESHX_CTRL_EVT_SET_CONSOLE_ROUTING = 0x08, /* Set serial multiplex console routing */
    MESHX_CTRL_EVT_GET_ELEMENT_STATE = 0x09, /* Get current element states */
} meshx_ctrl_evt_t;


/*********************************************************************
 *      FEATURE CONFIGURATION
 * *******************************************************************/

#define MESHX_NVS_STORE "meshx_store"
#define MESHX_PACKED    __attribute__((packed))

/**
 * @brief Structure to store mesh application data.
 */
typedef struct meshx_app_store
{
    uint16_t net_key_id; /**< Network key identifier */
    uint16_t node_addr;  /**< Node address */
} meshx_app_store_t;

/**
 * @brief Structure representing the device composition and elements.
 */
typedef struct dev_struct
{
    uint8_t uuid[16];                          /**< Device UUID */
    size_t element_idx;                        /**< Index of the current element */
    void *composition;                         /**< Device composition */
    MESHX_ELEMENT *elements;                   /**< Pointer to array of elements */
    size_t element_cnt;                        /**< Total number of elements */
    meshx_app_store_t meshx_store;             /**< Mesh application store */
} dev_struct_t;

#ifdef __cplusplus
}
#endif
