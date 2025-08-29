/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_cmn_def.h
 * @brief Common definitions for BLE Mesh models and opcodes in the MeshX framework.
 *
 * This header file contains macros and definitions for BLE Mesh models, opcodes,
 * and related constants used in the MeshX framework. It includes:
 * - Address type checks and constants.
 * - Model ID definitions for foundation and specification models.
 * - Opcode definitions for various BLE Mesh operations.
 * - Configuration status codes.
 *
 * These definitions are used to facilitate the implementation and interaction
 * with BLE Mesh models and their associated operations.
 *
 * @note Ensure that this file is included only once in a compilation unit by using `#pragma once`.
 */

#pragma once

#define MESHX_BIT(nr)                 (1UL << (nr))
#define MESHX_ARRAY_SIZE(_arr)        (sizeof(_arr) / sizeof((_arr)[0]))

#define MESHX_ADDR_IS_UNICAST(_addr)   ((_addr) && (_addr) < 0x8000)
#define MESHX_ADDR_BROADCAST(_addr)    (_addr == 0xFFFF)
#define MESHX_ADDR_IS_GROUP(_addr)     ((_addr) >= 0xC000 && (_addr) <= 0xFF00)

#define MESHX_ADDR_UNASSIGNED   0x0000
#define MESHX_KEY_UNUSED        0XFFFF

#define MESHX_MODEL_OP_1(b0)         (b0)
#define MESHX_MODEL_OP_2(b0, b1)     (((b0) << 8) | (b1))
#define MESHX_MODEL_OP_3(b0, cid)    ((((b0) << 16) | 0xC00000) | (cid))

/**
 * @brief BLE Mesh models related Model ID and Opcode definitions
 */

/*!< Foundation Models */
#define MESHX_MODEL_ID_CONFIG_SRV                            0x0000
#define MESHX_MODEL_ID_CONFIG_CLI                            0x0001
#define MESHX_MODEL_ID_HEALTH_SRV                            0x0002
#define MESHX_MODEL_ID_HEALTH_CLI                            0x0003
#define MESHX_MODEL_ID_RPR_SRV                               0x0004
#define MESHX_MODEL_ID_RPR_CLI                               0x0005
#define MESHX_MODEL_ID_DF_SRV                                0x0006
#define MESHX_MODEL_ID_DF_CLI                                0x0007
#define MESHX_MODEL_ID_BRC_SRV                               0x0008
#define MESHX_MODEL_ID_BRC_CLI                               0x0009
#define MESHX_MODEL_ID_PRB_SRV                               0x000A
#define MESHX_MODEL_ID_PRB_CLI                               0x000B
#define MESHX_MODEL_ID_ODP_SRV                               0x000C
#define MESHX_MODEL_ID_ODP_CLI                               0x000D
#define MESHX_MODEL_ID_SAR_SRV                               0x000E
#define MESHX_MODEL_ID_SAR_CLI                               0x000F
#define MESHX_MODEL_ID_AGG_SRV                               0x0010
#define MESHX_MODEL_ID_AGG_CLI                               0x0011
#define MESHX_MODEL_ID_LCD_SRV                               0x0012
#define MESHX_MODEL_ID_LCD_CLI                               0x0013
#define MESHX_MODEL_ID_SRPL_SRV                              0x0014
#define MESHX_MODEL_ID_SRPL_CLI                              0x0015

/*!< Models from the Mesh Model Specification */
#define MESHX_MODEL_ID_GEN_ONOFF_SRV                         0x1000
#define MESHX_MODEL_ID_GEN_ONOFF_CLI                         0x1001
#define MESHX_MODEL_ID_GEN_LEVEL_SRV                         0x1002
#define MESHX_MODEL_ID_GEN_LEVEL_CLI                         0x1003
#define MESHX_MODEL_ID_GEN_DEF_TRANS_TIME_SRV                0x1004
#define MESHX_MODEL_ID_GEN_DEF_TRANS_TIME_CLI                0x1005
#define MESHX_MODEL_ID_GEN_POWER_ONOFF_SRV                   0x1006
#define MESHX_MODEL_ID_GEN_POWER_ONOFF_SETUP_SRV             0x1007
#define MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI                   0x1008
#define MESHX_MODEL_ID_GEN_POWER_LEVEL_SRV                   0x1009
#define MESHX_MODEL_ID_GEN_POWER_LEVEL_SETUP_SRV             0x100a
#define MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI                   0x100b
#define MESHX_MODEL_ID_GEN_BATTERY_SRV                       0x100c
#define MESHX_MODEL_ID_GEN_BATTERY_CLI                       0x100d
#define MESHX_MODEL_ID_GEN_LOCATION_SRV                      0x100e
#define MESHX_MODEL_ID_GEN_LOCATION_SETUP_SRV                0x100f
#define MESHX_MODEL_ID_GEN_LOCATION_CLI                      0x1010
#define MESHX_MODEL_ID_GEN_ADMIN_PROP_SRV                    0x1011
#define MESHX_MODEL_ID_GEN_MANUFACTURER_PROP_SRV             0x1012
#define MESHX_MODEL_ID_GEN_USER_PROP_SRV                     0x1013
#define MESHX_MODEL_ID_GEN_CLIENT_PROP_SRV                   0x1014
#define MESHX_MODEL_ID_GEN_PROP_CLI                          0x1015
#define MESHX_MODEL_ID_SENSOR_SRV                            0x1100
#define MESHX_MODEL_ID_SENSOR_SETUP_SRV                      0x1101
#define MESHX_MODEL_ID_SENSOR_CLI                            0x1102
#define MESHX_MODEL_ID_TIME_SRV                              0x1200
#define MESHX_MODEL_ID_TIME_SETUP_SRV                        0x1201
#define MESHX_MODEL_ID_TIME_CLI                              0x1202
#define MESHX_MODEL_ID_SCENE_SRV                             0x1203
#define MESHX_MODEL_ID_SCENE_SETUP_SRV                       0x1204
#define MESHX_MODEL_ID_SCENE_CLI                             0x1205
#define MESHX_MODEL_ID_SCHEDULER_SRV                         0x1206
#define MESHX_MODEL_ID_SCHEDULER_SETUP_SRV                   0x1207
#define MESHX_MODEL_ID_SCHEDULER_CLI                         0x1208
#define MESHX_MODEL_ID_LIGHT_LIGHTNESS_SRV                   0x1300
#define MESHX_MODEL_ID_LIGHT_LIGHTNESS_SETUP_SRV             0x1301
#define MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI                   0x1302
#define MESHX_MODEL_ID_LIGHT_CTL_SRV                         0x1303
#define MESHX_MODEL_ID_LIGHT_CTL_SETUP_SRV                   0x1304
#define MESHX_MODEL_ID_LIGHT_CTL_CLI                         0x1305
#define MESHX_MODEL_ID_LIGHT_CTL_TEMP_SRV                    0x1306
#define MESHX_MODEL_ID_LIGHT_HSL_SRV                         0x1307
#define MESHX_MODEL_ID_LIGHT_HSL_SETUP_SRV                   0x1308
#define MESHX_MODEL_ID_LIGHT_HSL_CLI                         0x1309
#define MESHX_MODEL_ID_LIGHT_HSL_HUE_SRV                     0x130a
#define MESHX_MODEL_ID_LIGHT_HSL_SAT_SRV                     0x130b
#define MESHX_MODEL_ID_LIGHT_XYL_SRV                         0x130c
#define MESHX_MODEL_ID_LIGHT_XYL_SETUP_SRV                   0x130d
#define MESHX_MODEL_ID_LIGHT_XYL_CLI                         0x130e
#define MESHX_MODEL_ID_LIGHT_LC_SRV                          0x130f
#define MESHX_MODEL_ID_LIGHT_LC_SETUP_SRV                    0x1310
#define MESHX_MODEL_ID_LIGHT_LC_CLI                          0x1311
#define MESHX_MODEL_ID_MBT_SRV                               0x1400
#define MESHX_MODEL_ID_MBT_CLI                               0x1401

#define MESHX_MODEL_OP_BEACON_GET                            MESHX_MODEL_OP_2(0x80, 0x09) /*!< Config Beacon Get */
#define MESHX_MODEL_OP_COMPOSITION_DATA_GET                  MESHX_MODEL_OP_2(0x80, 0x08) /*!< Config Composition Data Get */
#define MESHX_MODEL_OP_DEFAULT_TTL_GET                       MESHX_MODEL_OP_2(0x80, 0x0C) /*!< Config Default TTL Get */
#define MESHX_MODEL_OP_GATT_PROXY_GET                        MESHX_MODEL_OP_2(0x80, 0x12) /*!< Config GATT Proxy Get */
#define MESHX_MODEL_OP_RELAY_GET                             MESHX_MODEL_OP_2(0x80, 0x26) /*!< Config Relay Get */
#define MESHX_MODEL_OP_MODEL_PUB_GET                         MESHX_MODEL_OP_2(0x80, 0x18) /*!< Config Model Publication Get */
#define MESHX_MODEL_OP_FRIEND_GET                            MESHX_MODEL_OP_2(0x80, 0x0F) /*!< Config Friend Get */
#define MESHX_MODEL_OP_HEARTBEAT_PUB_GET                     MESHX_MODEL_OP_2(0x80, 0x38) /*!< Config Heartbeat Publication Get */
#define MESHX_MODEL_OP_HEARTBEAT_SUB_GET                     MESHX_MODEL_OP_2(0x80, 0x3a) /*!< Config Heartbeat Subscription Get */
#define MESHX_MODEL_OP_NET_KEY_GET                           MESHX_MODEL_OP_2(0x80, 0x42) /*!< Config NetKey Get */
#define MESHX_MODEL_OP_APP_KEY_GET                           MESHX_MODEL_OP_2(0x80, 0x01) /*!< Config AppKey Get */
#define MESHX_MODEL_OP_NODE_IDENTITY_GET                     MESHX_MODEL_OP_2(0x80, 0x46) /*!< Config Node Identity Get */
#define MESHX_MODEL_OP_SIG_MODEL_SUB_GET                     MESHX_MODEL_OP_2(0x80, 0x29) /*!< Config SIG Model Subscription Get */
#define MESHX_MODEL_OP_VENDOR_MODEL_SUB_GET                  MESHX_MODEL_OP_2(0x80, 0x2B) /*!< Config Vendor Model Subscription Get */
#define MESHX_MODEL_OP_SIG_MODEL_APP_GET                     MESHX_MODEL_OP_2(0x80, 0x4B) /*!< Config SIG Model App Get */
#define MESHX_MODEL_OP_VENDOR_MODEL_APP_GET                  MESHX_MODEL_OP_2(0x80, 0x4D) /*!< Config Vendor Model App Get */
#define MESHX_MODEL_OP_KEY_REFRESH_PHASE_GET                 MESHX_MODEL_OP_2(0x80, 0x15) /*!< Config Key Refresh Phase Get */
#define MESHX_MODEL_OP_LPN_POLLTIMEOUT_GET                   MESHX_MODEL_OP_2(0x80, 0x2D) /*!< Config Low Power Node PollTimeout Get */
#define MESHX_MODEL_OP_NETWORK_TRANSMIT_GET                  MESHX_MODEL_OP_2(0x80, 0x23) /*!< Config Network Transmit Get */

#define MESHX_MODEL_OP_BEACON_SET                            MESHX_MODEL_OP_2(0x80, 0x0A) /*!< Config Beacon Set */
#define MESHX_MODEL_OP_DEFAULT_TTL_SET                       MESHX_MODEL_OP_2(0x80, 0x0D) /*!< Config Default TTL Set */
#define MESHX_MODEL_OP_GATT_PROXY_SET                        MESHX_MODEL_OP_2(0x80, 0x13) /*!< Config GATT Proxy Set */
#define MESHX_MODEL_OP_RELAY_SET                             MESHX_MODEL_OP_2(0x80, 0x27) /*!< Config Relay Set */
#define MESHX_MODEL_OP_MODEL_PUB_SET                         MESHX_MODEL_OP_1(0x03)       /*!< Config Model Publication Set */
#define MESHX_MODEL_OP_MODEL_SUB_ADD                         MESHX_MODEL_OP_2(0x80, 0x1B) /*!< Config Model Subscription Add */
#define MESHX_MODEL_OP_MODEL_SUB_VIRTUAL_ADDR_ADD            MESHX_MODEL_OP_2(0x80, 0x20) /*!< Config Model Subscription Virtual Address Add */
#define MESHX_MODEL_OP_MODEL_SUB_DELETE                      MESHX_MODEL_OP_2(0x80, 0x1C) /*!< Config Model Subscription Delete */
#define MESHX_MODEL_OP_MODEL_SUB_VIRTUAL_ADDR_DELETE         MESHX_MODEL_OP_2(0x80, 0x21) /*!< Config Model Subscription Virtual Address Delete */
#define MESHX_MODEL_OP_MODEL_SUB_OVERWRITE                   MESHX_MODEL_OP_2(0x80, 0x1E) /*!< Config Model Subscription Overwrite */
#define MESHX_MODEL_OP_MODEL_SUB_VIRTUAL_ADDR_OVERWRITE      MESHX_MODEL_OP_2(0x80, 0x22) /*!< Config Model Subscription Virtual Address Overwrite */
#define MESHX_MODEL_OP_NET_KEY_ADD                           MESHX_MODEL_OP_2(0x80, 0x40) /*!< Config NetKey Add */
#define MESHX_MODEL_OP_APP_KEY_ADD                           MESHX_MODEL_OP_1(0x00)       /*!< Config AppKey Add */
#define MESHX_MODEL_OP_MODEL_APP_BIND                        MESHX_MODEL_OP_2(0x80, 0x3D) /*!< Config Model App Bind */
#define MESHX_MODEL_OP_NODE_RESET                            MESHX_MODEL_OP_2(0x80, 0x49) /*!< Config Node Reset */
#define MESHX_MODEL_OP_FRIEND_SET                            MESHX_MODEL_OP_2(0x80, 0x10) /*!< Config Friend Set */
#define MESHX_MODEL_OP_HEARTBEAT_PUB_SET                     MESHX_MODEL_OP_2(0x80, 0x39) /*!< Config Heartbeat Publication Set */
#define MESHX_MODEL_OP_HEARTBEAT_SUB_SET                     MESHX_MODEL_OP_2(0x80, 0x3B) /*!< Config Heartbeat Subscription Set */
#define MESHX_MODEL_OP_NET_KEY_UPDATE                        MESHX_MODEL_OP_2(0x80, 0x45) /*!< Config NetKey Update */
#define MESHX_MODEL_OP_NET_KEY_DELETE                        MESHX_MODEL_OP_2(0x80, 0x41) /*!< Config NetKey Delete */
#define MESHX_MODEL_OP_APP_KEY_UPDATE                        MESHX_MODEL_OP_1(0x01)       /*!< Config AppKey Update */
#define MESHX_MODEL_OP_APP_KEY_DELETE                        MESHX_MODEL_OP_2(0x80, 0x00) /*!< Config AppKey Delete */
#define MESHX_MODEL_OP_NODE_IDENTITY_SET                     MESHX_MODEL_OP_2(0x80, 0x47) /*!< Config Node Identity Set */
#define MESHX_MODEL_OP_KEY_REFRESH_PHASE_SET                 MESHX_MODEL_OP_2(0x80, 0x16) /*!< Config Key Refresh Phase Set */
#define MESHX_MODEL_OP_MODEL_PUB_VIRTUAL_ADDR_SET            MESHX_MODEL_OP_2(0x80, 0x1A) /*!< Config Model Publication Virtual Address Set */
#define MESHX_MODEL_OP_MODEL_SUB_DELETE_ALL                  MESHX_MODEL_OP_2(0x80, 0x1D) /*!< Config Model Subscription Delete All */
#define MESHX_MODEL_OP_MODEL_APP_UNBIND                      MESHX_MODEL_OP_2(0x80, 0x3F) /*!< Config Model App Unbind */
#define MESHX_MODEL_OP_NETWORK_TRANSMIT_SET                  MESHX_MODEL_OP_2(0x80, 0x24) /*!< Config Network Transmit Set */

#define MESHX_MODEL_OP_BEACON_STATUS                         MESHX_MODEL_OP_2(0x80, 0x0B)
#define MESHX_MODEL_OP_COMPOSITION_DATA_STATUS               MESHX_MODEL_OP_1(0x02)
#define MESHX_MODEL_OP_DEFAULT_TTL_STATUS                    MESHX_MODEL_OP_2(0x80, 0x0E)
#define MESHX_MODEL_OP_GATT_PROXY_STATUS                     MESHX_MODEL_OP_2(0x80, 0x14)
#define MESHX_MODEL_OP_RELAY_STATUS                          MESHX_MODEL_OP_2(0x80, 0x28)
#define MESHX_MODEL_OP_MODEL_PUB_STATUS                      MESHX_MODEL_OP_2(0x80, 0x19)
#define MESHX_MODEL_OP_MODEL_SUB_STATUS                      MESHX_MODEL_OP_2(0x80, 0x1F)
#define MESHX_MODEL_OP_SIG_MODEL_SUB_LIST                    MESHX_MODEL_OP_2(0x80, 0x2A)
#define MESHX_MODEL_OP_VENDOR_MODEL_SUB_LIST                 MESHX_MODEL_OP_2(0x80, 0x2C)
#define MESHX_MODEL_OP_NET_KEY_STATUS                        MESHX_MODEL_OP_2(0x80, 0x44)
#define MESHX_MODEL_OP_NET_KEY_LIST                          MESHX_MODEL_OP_2(0x80, 0x43)
#define MESHX_MODEL_OP_APP_KEY_STATUS                        MESHX_MODEL_OP_2(0x80, 0x03)
#define MESHX_MODEL_OP_APP_KEY_LIST                          MESHX_MODEL_OP_2(0x80, 0x02)
#define MESHX_MODEL_OP_NODE_IDENTITY_STATUS                  MESHX_MODEL_OP_2(0x80, 0x48)
#define MESHX_MODEL_OP_MODEL_APP_STATUS                      MESHX_MODEL_OP_2(0x80, 0x3E)
#define MESHX_MODEL_OP_SIG_MODEL_APP_LIST                    MESHX_MODEL_OP_2(0x80, 0x4C)
#define MESHX_MODEL_OP_VENDOR_MODEL_APP_LIST                 MESHX_MODEL_OP_2(0x80, 0x4E)
#define MESHX_MODEL_OP_NODE_RESET_STATUS                     MESHX_MODEL_OP_2(0x80, 0x4A)
#define MESHX_MODEL_OP_FRIEND_STATUS                         MESHX_MODEL_OP_2(0x80, 0x11)
#define MESHX_MODEL_OP_KEY_REFRESH_PHASE_STATUS              MESHX_MODEL_OP_2(0x80, 0x17)
#define MESHX_MODEL_OP_HEARTBEAT_PUB_STATUS                  MESHX_MODEL_OP_1(0x06)
#define MESHX_MODEL_OP_HEARTBEAT_SUB_STATUS                  MESHX_MODEL_OP_2(0x80, 0x3C)
#define MESHX_MODEL_OP_LPN_POLLTIMEOUT_STATUS                MESHX_MODEL_OP_2(0x80, 0x2E)
#define MESHX_MODEL_OP_NETWORK_TRANSMIT_STATUS               MESHX_MODEL_OP_2(0x80, 0x25)

#define MESHX_CFG_STATUS_SUCCESS                             0x00
#define MESHX_CFG_STATUS_INVALID_ADDRESS                     0x01
#define MESHX_CFG_STATUS_INVALID_MODEL                       0x02
#define MESHX_CFG_STATUS_INVALID_APPKEY                      0x03
#define MESHX_CFG_STATUS_INVALID_NETKEY                      0x04
#define MESHX_CFG_STATUS_INSUFFICIENT_RESOURCES              0x05
#define MESHX_CFG_STATUS_KEY_INDEX_ALREADY_STORED            0x06
#define MESHX_CFG_STATUS_INVALID_PUBLISH_PARAMETERS          0x07
#define MESHX_CFG_STATUS_NOT_A_SUBSCRIBE_MODEL               0x08
#define MESHX_CFG_STATUS_STORAGE_FAILURE                     0x09
#define MESHX_CFG_STATUS_FEATURE_NOT_SUPPORTED               0x0A
#define MESHX_CFG_STATUS_CANNOT_UPDATE                       0x0B
#define MESHX_CFG_STATUS_CANNOT_REMOVE                       0x0C
#define MESHX_CFG_STATUS_CANNOT_BIND                         0x0D
#define MESHX_CFG_STATUS_TEMP_UNABLE_TO_CHANGE_STATE         0x0E
#define MESHX_CFG_STATUS_CANNOT_SET                          0x0F
#define MESHX_CFG_STATUS_UNSPECIFIED_ERROR                   0x10
#define MESHX_CFG_STATUS_INVALID_BINDING                     0x11
#define MESHX_CFG_STATUS_INVALID_PATH_ENTRY                  0x12
#define MESHX_CFG_STATUS_CANNOT_GET                          0x13
#define MESHX_CFG_STATUS_OBSOLETE_INFO                       0x14
#define MESHX_CFG_STATUS_INVALID_BEARER                      0x15

#define MESHX_MODEL_OP_HEALTH_FAULT_GET                      MESHX_MODEL_OP_2(0x80, 0x31) /*!< Health Fault Get */
#define MESHX_MODEL_OP_HEALTH_PERIOD_GET                     MESHX_MODEL_OP_2(0x80, 0x34) /*!< Health Period Get */
#define MESHX_MODEL_OP_ATTENTION_GET                         MESHX_MODEL_OP_2(0x80, 0x04) /*!< Health Attention Get */

#define MESHX_MODEL_OP_HEALTH_FAULT_CLEAR                    MESHX_MODEL_OP_2(0x80, 0x2F) /*!< Health Fault Clear */
#define MESHX_MODEL_OP_HEALTH_FAULT_CLEAR_UNACK              MESHX_MODEL_OP_2(0x80, 0x30) /*!< Health Fault Clear Unacknowledged */
#define MESHX_MODEL_OP_HEALTH_FAULT_TEST                     MESHX_MODEL_OP_2(0x80, 0x32) /*!< Health Fault Test */
#define MESHX_MODEL_OP_HEALTH_FAULT_TEST_UNACK               MESHX_MODEL_OP_2(0x80, 0x33) /*!< Health Fault Test Unacknowledged */
#define MESHX_MODEL_OP_HEALTH_PERIOD_SET                     MESHX_MODEL_OP_2(0x80, 0x35) /*!< Health Period Set */
#define MESHX_MODEL_OP_HEALTH_PERIOD_SET_UNACK               MESHX_MODEL_OP_2(0x80, 0x36) /*!< Health Period Set Unacknowledged */
#define MESHX_MODEL_OP_ATTENTION_SET                         MESHX_MODEL_OP_2(0x80, 0x05) /*!< Health Attention Set */
#define MESHX_MODEL_OP_ATTENTION_SET_UNACK                   MESHX_MODEL_OP_2(0x80, 0x06) /*!< Health Attention Set Unacknowledged */

#define MESHX_MODEL_OP_HEALTH_CURRENT_STATUS                 MESHX_MODEL_OP_1(0x04)
#define MESHX_MODEL_OP_HEALTH_FAULT_STATUS                   MESHX_MODEL_OP_1(0x05)
#define MESHX_MODEL_OP_HEALTH_PERIOD_STATUS                  MESHX_MODEL_OP_2(0x80, 0x37)
#define MESHX_MODEL_OP_ATTENTION_STATUS                      MESHX_MODEL_OP_2(0x80, 0x07)

/*!< Generic OnOff Message Opcode */
#define MESHX_MODEL_OP_GEN_ONOFF_GET                         MESHX_MODEL_OP_2(0x82, 0x01)
#define MESHX_MODEL_OP_GEN_ONOFF_SET                         MESHX_MODEL_OP_2(0x82, 0x02)
#define MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x03)
#define MESHX_MODEL_OP_GEN_ONOFF_STATUS                      MESHX_MODEL_OP_2(0x82, 0x04)

/*!< Generic Level Message Opcode */
#define MESHX_MODEL_OP_GEN_LEVEL_GET                         MESHX_MODEL_OP_2(0x82, 0x05)
#define MESHX_MODEL_OP_GEN_LEVEL_SET                         MESHX_MODEL_OP_2(0x82, 0x06)
#define MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x07)
#define MESHX_MODEL_OP_GEN_LEVEL_STATUS                      MESHX_MODEL_OP_2(0x82, 0x08)
#define MESHX_MODEL_OP_GEN_DELTA_SET                         MESHX_MODEL_OP_2(0x82, 0x09)
#define MESHX_MODEL_OP_GEN_DELTA_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x0A)
#define MESHX_MODEL_OP_GEN_MOVE_SET                          MESHX_MODEL_OP_2(0x82, 0x0B)
#define MESHX_MODEL_OP_GEN_MOVE_SET_UNACK                    MESHX_MODEL_OP_2(0x82, 0x0C)

/*!< Generic Default Transition Time Message Opcode */
#define MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_GET                MESHX_MODEL_OP_2(0x82, 0x0D)
#define MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_SET                MESHX_MODEL_OP_2(0x82, 0x0E)
#define MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_SET_UNACK          MESHX_MODEL_OP_2(0x82, 0x0F)
#define MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_STATUS             MESHX_MODEL_OP_2(0x82, 0x10)

/*!< Generic Power OnOff Message Opcode */
#define MESHX_MODEL_OP_GEN_ONPOWERUP_GET                     MESHX_MODEL_OP_2(0x82, 0x11)
#define MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS                  MESHX_MODEL_OP_2(0x82, 0x12)

/*!< Generic Power OnOff Setup Message Opcode */
#define MESHX_MODEL_OP_GEN_ONPOWERUP_SET                     MESHX_MODEL_OP_2(0x82, 0x13)
#define MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK               MESHX_MODEL_OP_2(0x82, 0x14)

/*!< Generic Power Level Message Opcode */
#define MESHX_MODEL_OP_GEN_POWER_LEVEL_GET                   MESHX_MODEL_OP_2(0x82, 0x15)
#define MESHX_MODEL_OP_GEN_POWER_LEVEL_SET                   MESHX_MODEL_OP_2(0x82, 0x16)
#define MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK             MESHX_MODEL_OP_2(0x82, 0x17)
#define MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS                MESHX_MODEL_OP_2(0x82, 0x18)
#define MESHX_MODEL_OP_GEN_POWER_LAST_GET                    MESHX_MODEL_OP_2(0x82, 0x19)
#define MESHX_MODEL_OP_GEN_POWER_LAST_STATUS                 MESHX_MODEL_OP_2(0x82, 0x1A)
#define MESHX_MODEL_OP_GEN_POWER_DEFAULT_GET                 MESHX_MODEL_OP_2(0x82, 0x1B)
#define MESHX_MODEL_OP_GEN_POWER_DEFAULT_STATUS              MESHX_MODEL_OP_2(0x82, 0x1C)
#define MESHX_MODEL_OP_GEN_POWER_RANGE_GET                   MESHX_MODEL_OP_2(0x82, 0x1D)
#define MESHX_MODEL_OP_GEN_POWER_RANGE_STATUS                MESHX_MODEL_OP_2(0x82, 0x1E)

/*!< Generic Power Level Setup Message Opcode */
#define MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET                 MESHX_MODEL_OP_2(0x82, 0x1F)
#define MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET_UNACK           MESHX_MODEL_OP_2(0x82, 0x20)
#define MESHX_MODEL_OP_GEN_POWER_RANGE_SET                   MESHX_MODEL_OP_2(0x82, 0x21)
#define MESHX_MODEL_OP_GEN_POWER_RANGE_SET_UNACK             MESHX_MODEL_OP_2(0x82, 0x22)

/*!< Generic Battery Message Opcode */
#define MESHX_MODEL_OP_GEN_BATTERY_GET                       MESHX_MODEL_OP_2(0x82, 0x23)
#define MESHX_MODEL_OP_GEN_BATTERY_STATUS                    MESHX_MODEL_OP_2(0x82, 0x24)

/*!< Generic Location Message Opcode */
#define MESHX_MODEL_OP_GEN_LOC_GLOBAL_GET                    MESHX_MODEL_OP_2(0x82, 0x25)
#define MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS                 MESHX_MODEL_OP_1(0x40)
#define MESHX_MODEL_OP_GEN_LOC_LOCAL_GET                     MESHX_MODEL_OP_2(0x82, 0x26)
#define MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS                  MESHX_MODEL_OP_2(0x82, 0x27)

/*!< Generic Location Setup Message Opcode */
#define MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET                    MESHX_MODEL_OP_1(0x41)
#define MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET_UNACK              MESHX_MODEL_OP_1(0x42)
#define MESHX_MODEL_OP_GEN_LOC_LOCAL_SET                     MESHX_MODEL_OP_2(0x82, 0x28)
#define MESHX_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK               MESHX_MODEL_OP_2(0x82, 0x29)

/*!< Generic Manufacturer Property Message Opcode */
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_GET       MESHX_MODEL_OP_2(0x82, 0x2A)
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS    MESHX_MODEL_OP_1(0x43)
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_GET         MESHX_MODEL_OP_2(0x82, 0x2B)
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_SET         MESHX_MODEL_OP_1(0x44)
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_SET_UNACK   MESHX_MODEL_OP_1(0x45)
#define MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS      MESHX_MODEL_OP_1(0x46)

/*!< Generic Admin Property Message Opcode */
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_GET              MESHX_MODEL_OP_2(0x82, 0x2C)
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_STATUS           MESHX_MODEL_OP_1(0x47)
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_GET                MESHX_MODEL_OP_2(0x82, 0x2D)
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_SET                MESHX_MODEL_OP_1(0x48)
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_SET_UNACK          MESHX_MODEL_OP_1(0x49)
#define MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_STATUS             MESHX_MODEL_OP_1(0x4A)

/*!< Generic User Property Message Opcode */
#define MESHX_MODEL_OP_GEN_USER_PROPERTIES_GET               MESHX_MODEL_OP_2(0x82, 0x2E)
#define MESHX_MODEL_OP_GEN_USER_PROPERTIES_STATUS            MESHX_MODEL_OP_1(0x4B)
#define MESHX_MODEL_OP_GEN_USER_PROPERTY_GET                 MESHX_MODEL_OP_2(0x82, 0x2F)
#define MESHX_MODEL_OP_GEN_USER_PROPERTY_SET                 MESHX_MODEL_OP_1(0x4C)
#define MESHX_MODEL_OP_GEN_USER_PROPERTY_SET_UNACK           MESHX_MODEL_OP_1(0x4D)
#define MESHX_MODEL_OP_GEN_USER_PROPERTY_STATUS              MESHX_MODEL_OP_1(0x4E)

/*!< Generic Client Property Message Opcode */
#define MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_GET             MESHX_MODEL_OP_1(0x4F)
#define MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_STATUS          MESHX_MODEL_OP_1(0x50)

/*!< Sensor Message Opcode */
#define MESHX_MODEL_OP_SENSOR_DESCRIPTOR_GET                 MESHX_MODEL_OP_2(0x82, 0x30)
#define MESHX_MODEL_OP_SENSOR_DESCRIPTOR_STATUS              MESHX_MODEL_OP_1(0x51)
#define MESHX_MODEL_OP_SENSOR_GET                            MESHX_MODEL_OP_2(0x82, 0x31)
#define MESHX_MODEL_OP_SENSOR_STATUS                         MESHX_MODEL_OP_1(0x52)
#define MESHX_MODEL_OP_SENSOR_COLUMN_GET                     MESHX_MODEL_OP_2(0x82, 0x32)
#define MESHX_MODEL_OP_SENSOR_COLUMN_STATUS                  MESHX_MODEL_OP_1(0x53)
#define MESHX_MODEL_OP_SENSOR_SERIES_GET                     MESHX_MODEL_OP_2(0x82, 0x33)
#define MESHX_MODEL_OP_SENSOR_SERIES_STATUS                  MESHX_MODEL_OP_1(0x54)

/*!< Sensor Setup Message Opcode */
#define MESHX_MODEL_OP_SENSOR_CADENCE_GET                    MESHX_MODEL_OP_2(0x82, 0x34)
#define MESHX_MODEL_OP_SENSOR_CADENCE_SET                    MESHX_MODEL_OP_1(0x55)
#define MESHX_MODEL_OP_SENSOR_CADENCE_SET_UNACK              MESHX_MODEL_OP_1(0x56)
#define MESHX_MODEL_OP_SENSOR_CADENCE_STATUS                 MESHX_MODEL_OP_1(0x57)
#define MESHX_MODEL_OP_SENSOR_SETTINGS_GET                   MESHX_MODEL_OP_2(0x82, 0x35)
#define MESHX_MODEL_OP_SENSOR_SETTINGS_STATUS                MESHX_MODEL_OP_1(0x58)
#define MESHX_MODEL_OP_SENSOR_SETTING_GET                    MESHX_MODEL_OP_2(0x82, 0x36)
#define MESHX_MODEL_OP_SENSOR_SETTING_SET                    MESHX_MODEL_OP_1(0x59)
#define MESHX_MODEL_OP_SENSOR_SETTING_SET_UNACK              MESHX_MODEL_OP_1(0x5A)
#define MESHX_MODEL_OP_SENSOR_SETTING_STATUS                 MESHX_MODEL_OP_1(0x5B)

/*!< Time Message Opcode */
#define MESHX_MODEL_OP_TIME_GET                              MESHX_MODEL_OP_2(0x82, 0x37)
#define MESHX_MODEL_OP_TIME_SET                              MESHX_MODEL_OP_1(0x5C)
#define MESHX_MODEL_OP_TIME_STATUS                           MESHX_MODEL_OP_1(0x5D)
#define MESHX_MODEL_OP_TIME_ROLE_GET                         MESHX_MODEL_OP_2(0x82, 0x38)
#define MESHX_MODEL_OP_TIME_ROLE_SET                         MESHX_MODEL_OP_2(0x82, 0x39)
#define MESHX_MODEL_OP_TIME_ROLE_STATUS                      MESHX_MODEL_OP_2(0x82, 0x3A)
#define MESHX_MODEL_OP_TIME_ZONE_GET                         MESHX_MODEL_OP_2(0x82, 0x3B)
#define MESHX_MODEL_OP_TIME_ZONE_SET                         MESHX_MODEL_OP_2(0x82, 0x3C)
#define MESHX_MODEL_OP_TIME_ZONE_STATUS                      MESHX_MODEL_OP_2(0x82, 0x3D)
#define MESHX_MODEL_OP_TAI_UTC_DELTA_GET                     MESHX_MODEL_OP_2(0x82, 0x3E)
#define MESHX_MODEL_OP_TAI_UTC_DELTA_SET                     MESHX_MODEL_OP_2(0x82, 0x3F)
#define MESHX_MODEL_OP_TAI_UTC_DELTA_STATUS                  MESHX_MODEL_OP_2(0x82, 0x40)

/*!< Scene Message Opcode */
#define MESHX_MODEL_OP_SCENE_GET                             MESHX_MODEL_OP_2(0x82, 0x41)
#define MESHX_MODEL_OP_SCENE_RECALL                          MESHX_MODEL_OP_2(0x82, 0x42)
#define MESHX_MODEL_OP_SCENE_RECALL_UNACK                    MESHX_MODEL_OP_2(0x82, 0x43)
#define MESHX_MODEL_OP_SCENE_STATUS                          MESHX_MODEL_OP_1(0x5E)
#define MESHX_MODEL_OP_SCENE_REGISTER_GET                    MESHX_MODEL_OP_2(0x82, 0x44)
#define MESHX_MODEL_OP_SCENE_REGISTER_STATUS                 MESHX_MODEL_OP_2(0x82, 0x45)

/*!< Scene Setup Message Opcode */
#define MESHX_MODEL_OP_SCENE_STORE                           MESHX_MODEL_OP_2(0x82, 0x46)
#define MESHX_MODEL_OP_SCENE_STORE_UNACK                     MESHX_MODEL_OP_2(0x82, 0x47)
#define MESHX_MODEL_OP_SCENE_DELETE                          MESHX_MODEL_OP_2(0x82, 0x9E)
#define MESHX_MODEL_OP_SCENE_DELETE_UNACK                    MESHX_MODEL_OP_2(0x82, 0x9F)

/*!< Scheduler Message Opcode */
#define MESHX_MODEL_OP_SCHEDULER_ACT_GET                     MESHX_MODEL_OP_2(0x82, 0x48)
#define MESHX_MODEL_OP_SCHEDULER_ACT_STATUS                  MESHX_MODEL_OP_1(0x5F)
#define MESHX_MODEL_OP_SCHEDULER_GET                         MESHX_MODEL_OP_2(0x82, 0x49)
#define MESHX_MODEL_OP_SCHEDULER_STATUS                      MESHX_MODEL_OP_2(0x82, 0x4A)

/*!< Scheduler Setup Message Opcode */
#define MESHX_MODEL_OP_SCHEDULER_ACT_SET                     MESHX_MODEL_OP_1(0x60)
#define MESHX_MODEL_OP_SCHEDULER_ACT_SET_UNACK               MESHX_MODEL_OP_1(0x61)

/*!< Light Lightness Message Opcode */
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_GET                   MESHX_MODEL_OP_2(0x82, 0x4B)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET                   MESHX_MODEL_OP_2(0x82, 0x4C)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK             MESHX_MODEL_OP_2(0x82, 0x4D)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_STATUS                MESHX_MODEL_OP_2(0x82, 0x4E)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_GET            MESHX_MODEL_OP_2(0x82, 0x4F)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET            MESHX_MODEL_OP_2(0x82, 0x50)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET_UNACK      MESHX_MODEL_OP_2(0x82, 0x51)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_STATUS         MESHX_MODEL_OP_2(0x82, 0x52)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LAST_GET              MESHX_MODEL_OP_2(0x82, 0x53)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_LAST_STATUS           MESHX_MODEL_OP_2(0x82, 0x54)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_GET           MESHX_MODEL_OP_2(0x82, 0x55)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_STATUS        MESHX_MODEL_OP_2(0x82, 0x56)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_GET             MESHX_MODEL_OP_2(0x82, 0x57)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_STATUS          MESHX_MODEL_OP_2(0x82, 0x58)

/*!< Light Lightness Setup Message Opcode */
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET           MESHX_MODEL_OP_2(0x82, 0x59)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET_UNACK     MESHX_MODEL_OP_2(0x82, 0x5A)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET             MESHX_MODEL_OP_2(0x82, 0x5B)
#define MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET_UNACK       MESHX_MODEL_OP_2(0x82, 0x5C)

/*!< Light CTL Message Opcode */
#define MESHX_MODEL_OP_LIGHT_CTL_GET                         MESHX_MODEL_OP_2(0x82, 0x5D)
#define MESHX_MODEL_OP_LIGHT_CTL_SET                         MESHX_MODEL_OP_2(0x82, 0x5E)
#define MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x5F)
#define MESHX_MODEL_OP_LIGHT_CTL_STATUS                      MESHX_MODEL_OP_2(0x82, 0x60)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET             MESHX_MODEL_OP_2(0x82, 0x61)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET       MESHX_MODEL_OP_2(0x82, 0x62)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS    MESHX_MODEL_OP_2(0x82, 0x63)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET             MESHX_MODEL_OP_2(0x82, 0x64)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK       MESHX_MODEL_OP_2(0x82, 0x65)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS          MESHX_MODEL_OP_2(0x82, 0x66)
#define MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_GET                 MESHX_MODEL_OP_2(0x82, 0x67)
#define MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS              MESHX_MODEL_OP_2(0x82, 0x68)

/*!< Light CTL Setup Message Opcode */
#define MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET                 MESHX_MODEL_OP_2(0x82, 0x69)
#define MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK           MESHX_MODEL_OP_2(0x82, 0x6A)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET       MESHX_MODEL_OP_2(0x82, 0x6B)
#define MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK MESHX_MODEL_OP_2(0x82, 0x6C)

/*!< Light HSL Message Opcode */
#define MESHX_MODEL_OP_LIGHT_HSL_GET                         MESHX_MODEL_OP_2(0x82, 0x6D)
#define MESHX_MODEL_OP_LIGHT_HSL_HUE_GET                     MESHX_MODEL_OP_2(0x82, 0x6E)
#define MESHX_MODEL_OP_LIGHT_HSL_HUE_SET                     MESHX_MODEL_OP_2(0x82, 0x6F)
#define MESHX_MODEL_OP_LIGHT_HSL_HUE_SET_UNACK               MESHX_MODEL_OP_2(0x82, 0x70)
#define MESHX_MODEL_OP_LIGHT_HSL_HUE_STATUS                  MESHX_MODEL_OP_2(0x82, 0x71)
#define MESHX_MODEL_OP_LIGHT_HSL_SATURATION_GET              MESHX_MODEL_OP_2(0x82, 0x72)
#define MESHX_MODEL_OP_LIGHT_HSL_SATURATION_SET              MESHX_MODEL_OP_2(0x82, 0x73)
#define MESHX_MODEL_OP_LIGHT_HSL_SATURATION_SET_UNACK        MESHX_MODEL_OP_2(0x82, 0x74)
#define MESHX_MODEL_OP_LIGHT_HSL_SATURATION_STATUS           MESHX_MODEL_OP_2(0x82, 0x75)
#define MESHX_MODEL_OP_LIGHT_HSL_SET                         MESHX_MODEL_OP_2(0x82, 0x76)
#define MESHX_MODEL_OP_LIGHT_HSL_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x77)
#define MESHX_MODEL_OP_LIGHT_HSL_STATUS                      MESHX_MODEL_OP_2(0x82, 0x78)
#define MESHX_MODEL_OP_LIGHT_HSL_TARGET_GET                  MESHX_MODEL_OP_2(0x82, 0x79)
#define MESHX_MODEL_OP_LIGHT_HSL_TARGET_STATUS               MESHX_MODEL_OP_2(0x82, 0x7A)
#define MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_GET                 MESHX_MODEL_OP_2(0x82, 0x7B)
#define MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_STATUS              MESHX_MODEL_OP_2(0x82, 0x7C)
#define MESHX_MODEL_OP_LIGHT_HSL_RANGE_GET                   MESHX_MODEL_OP_2(0x82, 0x7D)
#define MESHX_MODEL_OP_LIGHT_HSL_RANGE_STATUS                MESHX_MODEL_OP_2(0x82, 0x7E)

/*!< Light HSL Setup Message Opcode */
#define MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_SET                 MESHX_MODEL_OP_2(0x82, 0x7F)
#define MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_SET_UNACK           MESHX_MODEL_OP_2(0x82, 0x80)
#define MESHX_MODEL_OP_LIGHT_HSL_RANGE_SET                   MESHX_MODEL_OP_2(0x82, 0x81)
#define MESHX_MODEL_OP_LIGHT_HSL_RANGE_SET_UNACK             MESHX_MODEL_OP_2(0x82, 0x82)

/*!< Light xyL Message Opcode */
#define MESHX_MODEL_OP_LIGHT_XYL_GET                         MESHX_MODEL_OP_2(0x82, 0x83)
#define MESHX_MODEL_OP_LIGHT_XYL_SET                         MESHX_MODEL_OP_2(0x82, 0x84)
#define MESHX_MODEL_OP_LIGHT_XYL_SET_UNACK                   MESHX_MODEL_OP_2(0x82, 0x85)
#define MESHX_MODEL_OP_LIGHT_XYL_STATUS                      MESHX_MODEL_OP_2(0x82, 0x86)
#define MESHX_MODEL_OP_LIGHT_XYL_TARGET_GET                  MESHX_MODEL_OP_2(0x82, 0x87)
#define MESHX_MODEL_OP_LIGHT_XYL_TARGET_STATUS               MESHX_MODEL_OP_2(0x82, 0x88)
#define MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_GET                 MESHX_MODEL_OP_2(0x82, 0x89)
#define MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_STATUS              MESHX_MODEL_OP_2(0x82, 0x8A)
#define MESHX_MODEL_OP_LIGHT_XYL_RANGE_GET                   MESHX_MODEL_OP_2(0x82, 0x8B)
#define MESHX_MODEL_OP_LIGHT_XYL_RANGE_STATUS                MESHX_MODEL_OP_2(0x82, 0x8C)

/*!< Light xyL Setup Message Opcode */
#define MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_SET                 MESHX_MODEL_OP_2(0x82, 0x8D)
#define MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_SET_UNACK           MESHX_MODEL_OP_2(0x82, 0x8E)
#define MESHX_MODEL_OP_LIGHT_XYL_RANGE_SET                   MESHX_MODEL_OP_2(0x82, 0x8F)
#define MESHX_MODEL_OP_LIGHT_XYL_RANGE_SET_UNACK             MESHX_MODEL_OP_2(0x82, 0x90)

/*!< Light Control Message Opcode */
#define MESHX_MODEL_OP_LIGHT_LC_MODE_GET                     MESHX_MODEL_OP_2(0x82, 0x91)
#define MESHX_MODEL_OP_LIGHT_LC_MODE_SET                     MESHX_MODEL_OP_2(0x82, 0x92)
#define MESHX_MODEL_OP_LIGHT_LC_MODE_SET_UNACK               MESHX_MODEL_OP_2(0x82, 0x93)
#define MESHX_MODEL_OP_LIGHT_LC_MODE_STATUS                  MESHX_MODEL_OP_2(0x82, 0x94)
#define MESHX_MODEL_OP_LIGHT_LC_OM_GET                       MESHX_MODEL_OP_2(0x82, 0x95)
#define MESHX_MODEL_OP_LIGHT_LC_OM_SET                       MESHX_MODEL_OP_2(0x82, 0x96)
#define MESHX_MODEL_OP_LIGHT_LC_OM_SET_UNACK                 MESHX_MODEL_OP_2(0x82, 0x97)
#define MESHX_MODEL_OP_LIGHT_LC_OM_STATUS                    MESHX_MODEL_OP_2(0x82, 0x98)
#define MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_GET              MESHX_MODEL_OP_2(0x82, 0x99)
#define MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET              MESHX_MODEL_OP_2(0x82, 0x9A)
#define MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET_UNACK        MESHX_MODEL_OP_2(0x82, 0x9B)
#define MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_STATUS           MESHX_MODEL_OP_2(0x82, 0x9C)
#define MESHX_MODEL_OP_LIGHT_LC_PROPERTY_GET                 MESHX_MODEL_OP_2(0x82, 0x9D)
#define MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET                 MESHX_MODEL_OP_1(0x62)
#define MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET_UNACK           MESHX_MODEL_OP_1(0x63)
#define MESHX_MODEL_OP_LIGHT_LC_PROPERTY_STATUS              MESHX_MODEL_OP_1(0x64)

#define MESHX_SETTINGS_UID_SIZE 20

/*!< This enum value is the event of node/provisioner/fast provisioning */
typedef enum {
    MESHX_PROV_REGISTER_COMP_EVT,                        /*!< Initialize BLE Mesh provisioning capabilities and internal data information completion event */
    MESHX_NODE_SET_UNPROV_DEV_NAME_COMP_EVT,             /*!< Set the unprovisioned device name completion event */
    MESHX_NODE_PROV_ENABLE_COMP_EVT,                     /*!< Enable node provisioning functionality completion event */
    MESHX_NODE_PROV_DISABLE_COMP_EVT,                    /*!< Disable node provisioning functionality completion event */
    MESHX_NODE_PROV_LINK_OPEN_EVT,                       /*!< Establish a BLE Mesh link event */
    MESHX_NODE_PROV_LINK_CLOSE_EVT,                      /*!< Close a BLE Mesh link event */
    MESHX_NODE_PROV_OOB_PUB_KEY_EVT,                     /*!< Generate Node input OOB public key event */
    MESHX_NODE_PROV_OUTPUT_NUMBER_EVT,                   /*!< Generate Node Output Number event */
    MESHX_NODE_PROV_OUTPUT_STRING_EVT,                   /*!< Generate Node Output String event */
    MESHX_NODE_PROV_INPUT_EVT,                           /*!< Event requiring the user to input a number or string */
    MESHX_NODE_PROV_COMPLETE_EVT,                        /*!< Provisioning done event */
    MESHX_NODE_PROV_RESET_EVT,                           /*!< Provisioning reset event */
    MESHX_NODE_PROV_SET_OOB_PUB_KEY_COMP_EVT,            /*!< Node set oob public key completion event */
    MESHX_NODE_PROV_INPUT_NUMBER_COMP_EVT,               /*!< Node input number completion event */
    MESHX_NODE_PROV_INPUT_STRING_COMP_EVT,               /*!< Node input string completion event */
    MESHX_NODE_PROXY_IDENTITY_ENABLE_COMP_EVT,           /*!< Enable BLE Mesh Proxy Identity advertising completion event */
    MESHX_NODE_PRIVATE_PROXY_IDENTITY_ENABLE_COMP_EVT,   /*!< Enable BLE Mesh Private Proxy Identity advertising completion event */
    MESHX_NODE_PRIVATE_PROXY_IDENTITY_DISABLE_COMP_EVT,  /*!< Disable BLE Mesh Private Proxy Identity advertising completion event */
    MESHX_NODE_PROXY_GATT_ENABLE_COMP_EVT,               /*!< Enable BLE Mesh GATT Proxy Service completion event */
    MESHX_NODE_PROXY_GATT_DISABLE_COMP_EVT,              /*!< Disable BLE Mesh GATT Proxy Service completion event */
    MESHX_NODE_ADD_LOCAL_NET_KEY_COMP_EVT,               /*!< Node add NetKey locally completion event */
    MESHX_NODE_ADD_LOCAL_APP_KEY_COMP_EVT,               /*!< Node add AppKey locally completion event */
    MESHX_NODE_BIND_APP_KEY_TO_MODEL_COMP_EVT,           /*!< Node bind AppKey to model locally completion event */
    MESHX_PROVISIONER_PROV_ENABLE_COMP_EVT,              /*!< Provisioner enable provisioning functionality completion event */
    MESHX_PROVISIONER_PROV_DISABLE_COMP_EVT,             /*!< Provisioner disable provisioning functionality completion event */
    MESHX_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT,           /*!< Provisioner receives unprovisioned device beacon event */
    MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_EVT,         /*!< Provisioner read unprovisioned device OOB public key event */
    MESHX_PROVISIONER_PROV_INPUT_EVT,                    /*!< Provisioner input value for provisioning procedure event */
    MESHX_PROVISIONER_PROV_OUTPUT_EVT,                   /*!< Provisioner output value for provisioning procedure event */
    MESHX_PROVISIONER_PROV_LINK_OPEN_EVT,                /*!< Provisioner establish a BLE Mesh link event */
    MESHX_PROVISIONER_PROV_LINK_CLOSE_EVT,               /*!< Provisioner close a BLE Mesh link event */
    MESHX_PROVISIONER_PROV_COMPLETE_EVT,                 /*!< Provisioner provisioning done event */
    MESHX_PROVISIONER_CERT_BASED_PROV_START_EVT,         /*!< Provisioner initiate a certificate based provisioning */
    MESHX_PROVISIONER_RECV_PROV_RECORDS_LIST_EVT,        /*!< Provisioner receive provisioning records list event */
    MESHX_PROVISIONER_PROV_RECORD_RECV_COMP_EVT,         /*!< Provisioner receive provisioning record complete event */
    MESHX_PROVISIONER_SEND_PROV_RECORDS_GET_EVT,         /*!< Provisioner send provisioning records get to device event */
    MESHX_PROVISIONER_SEND_PROV_RECORD_REQUEST_EVT,      /*!< Provisioner send provisioning record request to device event */
    MESHX_PROVISIONER_SEND_PROV_INVITE_EVT,              /*!< Provisioner send provisioning invite to device event */
    MESHX_PROVISIONER_SEND_LINK_CLOSE_EVT,               /*!< Provisioner send link close to device event */
    MESHX_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT,           /*!< Provisioner add a device to the list which contains devices that are waiting/going to be provisioned completion event */
    MESHX_PROVISIONER_PROV_DEV_WITH_ADDR_COMP_EVT,       /*!< Provisioner start to provision an unprovisioned device completion event */
    MESHX_PROVISIONER_DELETE_DEV_COMP_EVT,               /*!< Provisioner delete a device from the list, close provisioning link with the device completion event */
    MESHX_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT,       /*!< Provisioner set the value to be compared with part of the unprovisioned device UUID completion event */
    MESHX_PROVISIONER_SET_PROV_DATA_INFO_COMP_EVT,       /*!< Provisioner set net_idx/flags/iv_index used for provisioning completion event */
    MESHX_PROVISIONER_SET_STATIC_OOB_VALUE_COMP_EVT,     /*!< Provisioner set static oob value used for provisioning completion event */
    MESHX_PROVISIONER_SET_PRIMARY_ELEM_ADDR_COMP_EVT,    /*!< Provisioner set unicast address of primary element completion event */
    MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_COMP_EVT,    /*!< Provisioner read unprovisioned device OOB public key completion event */
    MESHX_PROVISIONER_PROV_INPUT_NUMBER_COMP_EVT,        /*!< Provisioner input number completion event */
    MESHX_PROVISIONER_PROV_INPUT_STRING_COMP_EVT,        /*!< Provisioner input string completion event */
    MESHX_PROVISIONER_SET_NODE_NAME_COMP_EVT,            /*!< Provisioner set node name completion event */
    MESHX_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT,        /*!< Provisioner add local app key completion event */
    MESHX_PROVISIONER_UPDATE_LOCAL_APP_KEY_COMP_EVT,     /*!< Provisioner update local app key completion event */
    MESHX_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT,    /*!< Provisioner bind local model with local app key completion event */
    MESHX_PROVISIONER_ADD_LOCAL_NET_KEY_COMP_EVT,        /*!< Provisioner add local network key completion event */
    MESHX_PROVISIONER_UPDATE_LOCAL_NET_KEY_COMP_EVT,     /*!< Provisioner update local network key completion event */
    MESHX_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT,     /*!< Provisioner store node composition data completion event */
    MESHX_PROVISIONER_DELETE_NODE_WITH_UUID_COMP_EVT,    /*!< Provisioner delete node with uuid completion event */
    MESHX_PROVISIONER_DELETE_NODE_WITH_ADDR_COMP_EVT,    /*!< Provisioner delete node with unicast address completion event */
    MESHX_PROVISIONER_ENABLE_HEARTBEAT_RECV_COMP_EVT,     /*!< Provisioner start to receive heartbeat message completion event */
    MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_TYPE_COMP_EVT, /*!< Provisioner set the heartbeat filter type completion event */
    MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_INFO_COMP_EVT, /*!< Provisioner set the heartbeat filter information completion event */
    MESHX_PROVISIONER_RECV_HEARTBEAT_MESSAGE_EVT,         /*!< Provisioner receive heartbeat message event */
    MESHX_PROVISIONER_DIRECT_ERASE_SETTINGS_COMP_EVT,        /*!< Provisioner directly erase settings completion event */
    MESHX_PROVISIONER_OPEN_SETTINGS_WITH_INDEX_COMP_EVT,     /*!< Provisioner open settings with index completion event */
    MESHX_PROVISIONER_OPEN_SETTINGS_WITH_UID_COMP_EVT,       /*!< Provisioner open settings with user id completion event */
    MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_INDEX_COMP_EVT,    /*!< Provisioner close settings with index completion event */
    MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_UID_COMP_EVT,      /*!< Provisioner close settings with user id completion event */
    MESHX_PROVISIONER_DELETE_SETTINGS_WITH_INDEX_COMP_EVT,   /*!< Provisioner delete settings with index completion event */
    MESHX_PROVISIONER_DELETE_SETTINGS_WITH_UID_COMP_EVT,     /*!< Provisioner delete settings with user id completion event */
    MESHX_SET_FAST_PROV_INFO_COMP_EVT,                   /*!< Set fast provisioning information (e.g. unicast address range, net_idx, etc.) completion event */
    MESHX_SET_FAST_PROV_ACTION_COMP_EVT,                 /*!< Set fast provisioning action completion event */
    MESHX_HEARTBEAT_MESSAGE_RECV_EVT,                    /*!< Receive Heartbeat message event */
    MESHX_LPN_ENABLE_COMP_EVT,                           /*!< Enable Low Power Node completion event */
    MESHX_LPN_DISABLE_COMP_EVT,                          /*!< Disable Low Power Node completion event */
    MESHX_LPN_POLL_COMP_EVT,                             /*!< Low Power Node send Friend Poll completion event */
    MESHX_LPN_FRIENDSHIP_ESTABLISH_EVT,                  /*!< Low Power Node establishes friendship event */
    MESHX_LPN_FRIENDSHIP_TERMINATE_EVT,                  /*!< Low Power Node terminates friendship event */
    MESHX_FRIEND_FRIENDSHIP_ESTABLISH_EVT,               /*!< Friend Node establishes friendship event */
    MESHX_FRIEND_FRIENDSHIP_TERMINATE_EVT,               /*!< Friend Node terminates friendship event */
    MESHX_PROXY_CLIENT_RECV_ADV_PKT_EVT,                 /*!< Proxy Client receives Network ID advertising packet event */
    MESHX_PROXY_CLIENT_CONNECTED_EVT,                    /*!< Proxy Client establishes connection successfully event */
    MESHX_PROXY_CLIENT_DISCONNECTED_EVT,                 /*!< Proxy Client terminates connection successfully event */
    MESHX_PROXY_CLIENT_RECV_FILTER_STATUS_EVT,           /*!< Proxy Client receives Proxy Filter Status event */
    MESHX_PROXY_CLIENT_CONNECT_COMP_EVT,                 /*!< Proxy Client connect completion event */
    MESHX_PROXY_CLIENT_DISCONNECT_COMP_EVT,              /*!< Proxy Client disconnect completion event */
    MESHX_PROXY_CLIENT_SET_FILTER_TYPE_COMP_EVT,         /*!< Proxy Client set filter type completion event */
    MESHX_PROXY_CLIENT_ADD_FILTER_ADDR_COMP_EVT,         /*!< Proxy Client add filter address completion event */
    MESHX_PROXY_CLIENT_REMOVE_FILTER_ADDR_COMP_EVT,      /*!< Proxy Client remove filter address completion event */
    MESHX_PROXY_CLIENT_DIRECTED_PROXY_SET_COMP_EVT,      /*!< Proxy Client directed proxy set completion event */
    MESHX_PROXY_SERVER_CONNECTED_EVT,                    /*!< Proxy Server establishes connection successfully event */
    MESHX_PROXY_SERVER_DISCONNECTED_EVT,                 /*!< Proxy Server terminates connection successfully event */
    MESHX_PROXY_CLIENT_SEND_SOLIC_PDU_COMP_EVT,          /*!< Proxy Client send Solicitation PDU completion event */
    MESHX_MODEL_SUBSCRIBE_GROUP_ADDR_COMP_EVT,           /*!< Local model subscribes group address completion event */
    MESHX_MODEL_UNSUBSCRIBE_GROUP_ADDR_COMP_EVT,         /*!< Local model unsubscribes group address completion event */
    MESHX_DEINIT_MESH_COMP_EVT,                          /*!< De-initialize BLE Mesh stack completion event */
    MESHX_PROV_EVT_MAX,
} meshx_prov_cb_event_t;

typedef enum {
    MESHX_TYPE_PROV_CB,
    MESHX_TYPE_OUTPUT_NUM_CB,
    MESHX_TYPE_OUTPUT_STR_CB,
    MESHX_TYPE_INTPUT_CB,
    MESHX_TYPE_LINK_OPEN_CB,
    MESHX_TYPE_LINK_CLOSE_CB,
    MESHX_TYPE_COMPLETE_CB,
    MESHX_TYPE_RESET_CB,
} meshx_cb_type_t;

/*!< This enum value is provisioning authentication oob method */
typedef enum {
    MESHX_NO_OOB,
    MESHX_STATIC_OOB,
    MESHX_OUTPUT_OOB,
    MESHX_INPUT_OOB,
} meshx_oob_method_t;

/*!< This enum value is associated with bt_mesh_output_action_t in mesh_main.h */
typedef enum {
    MESHX_NO_OUTPUT       = 0,
    MESHX_BLINK           = MESHX_BIT(0),
    MESHX_BEEP            = MESHX_BIT(1),
    MESHX_VIBRATE         = MESHX_BIT(2),
    MESHX_DISPLAY_NUMBER  = MESHX_BIT(3),
    MESHX_DISPLAY_STRING  = MESHX_BIT(4),
} meshx_output_action_t;

/*!< This enum value is associated with bt_mesh_input_action_t in mesh_main.h */
typedef enum {
    MESHX_NO_INPUT      = 0,
    MESHX_PUSH          = MESHX_BIT(0),
    MESHX_TWIST         = MESHX_BIT(1),
    MESHX_ENTER_NUMBER  = MESHX_BIT(2),
    MESHX_ENTER_STRING  = MESHX_BIT(3),
} meshx_input_action_t;

typedef void* meshx_ptr_t;
typedef unsigned char meshx_addr_type_t;

#ifndef MESHX_BD_ADDR_LEN
#define MESHX_BD_ADDR_LEN     6
#endif

#ifndef MESHX_UUID_ADDR_LEN
#define MESHX_UUID_ADDR_LEN     16
#endif

typedef uint8_t meshx_bd_addr_t[MESHX_BD_ADDR_LEN];
typedef uint8_t meshx_uuid_addr_t[MESHX_UUID_ADDR_LEN];

#define MESHX_UUID_EMPTY (meshx_uuid_addr_t){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
