/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_gen_light_cli.h
 * @brief Header file for BLE Mesh Light Client functionality in the MeshX framework.
 *        This file contains declarations and includes required for implementing
 *        BLE Mesh Light Client operations.
 *
 * @author Pranjal Chanda
 */

#ifndef MESHX_BLE_MESH_LIGHT_CLI_H
#define MESHX_BLE_MESH_LIGHT_CLI_H

#include "../meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

typedef control_task_msg_handle_t meshx_gen_light_client_cb_t;

typedef enum
{
    MESHX_GEN_LIGHT_CLI_EVT_GET = MESHX_BIT(0),
    MESHX_GEN_LIGHT_CLI_EVT_SET = MESHX_BIT(1),
    MESHX_GEN_LIGHT_CLI_PUBLISH = MESHX_BIT(2),
    MESHX_GEN_LIGHT_CLI_TIMEOUT = MESHX_BIT(3),
    MESHX_GEN_LIGHT_CLI_EVT_ALL = (       \
            MESHX_GEN_LIGHT_CLI_EVT_GET | \
            MESHX_GEN_LIGHT_CLI_EVT_SET | \
            MESHX_GEN_LIGHT_CLI_PUBLISH | \
            MESHX_GEN_LIGHT_CLI_TIMEOUT)
} meshx_gen_light_cli_evt_t;

/**
 *  @brief Bluetooth Mesh Light Lightness Client Model Get and Set parameters structure.
 */

/** Parameters of Light Lightness Set */
typedef struct {
    bool     op_en;        /*!< Indicate if optional parameters are included */
    uint16_t lightness;    /*!< Target value of light lightness actual state */
    uint8_t  tid;          /*!< Transaction ID */
    uint8_t  trans_time;   /*!< Time to complete state transition (optional) */
    uint8_t  delay;        /*!< Indicate message execution delay (C.1) */
} meshx_light_lightness_set_t;

/** Parameters of Light Lightness Linear Set */
typedef struct {
    bool     op_en;        /*!< Indicate if optional parameters are included */
    uint16_t lightness;    /*!< Target value of light lightness linear state */
    uint8_t  tid;          /*!< Transaction ID */
    uint8_t  trans_time;   /*!< Time to complete state transition (optional) */
    uint8_t  delay;        /*!< Indicate message execution delay (C.1) */
} meshx_light_lightness_linear_set_t;

/** Parameter of Light Lightness Default Set */
typedef struct {
    uint16_t lightness;    /*!< The value of the Light Lightness Default state */
} meshx_light_lightness_default_set_t;

/** Parameters of Light Lightness Range Set */
typedef struct {
    uint16_t range_min;    /*!< Value of range min field of light lightness range state */
    uint16_t range_max;    /*!< Value of range max field of light lightness range state */
} meshx_light_lightness_range_set_t;

/** Parameters of Light CTL Set */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t ctl_lightness;    /*!< Target value of light ctl lightness state */
    uint16_t ctl_temperature;  /*!< Target value of light ctl temperature state */
    int16_t  ctl_delta_uv;     /*!< Target value of light ctl delta UV state */
    uint8_t  tid;              /*!< Transaction ID */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_ctl_set_t;

/** Parameters of Light CTL Temperature Set */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t ctl_temperature;  /*!< Target value of light ctl temperature state */
    int16_t  ctl_delta_uv;     /*!< Target value of light ctl delta UV state */
    uint8_t  tid;              /*!< Transaction ID */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_ctl_temperature_set_t;

/** Parameters of Light CTL Temperature Range Set */
typedef struct {
    uint16_t range_min;        /*!< Value of temperature range min field of light ctl temperature range state */
    uint16_t range_max;        /*!< Value of temperature range max field of light ctl temperature range state */
} meshx_light_ctl_temperature_range_set_t;

/** Parameters of Light CTL Default Set */
typedef struct {
    uint16_t lightness;        /*!< Value of light lightness default state */
    uint16_t temperature;      /*!< Value of light temperature default state */
    int16_t  delta_uv;         /*!< Value of light delta UV default state */
} meshx_light_ctl_default_set_t;

/** Parameters of Light HSL Set */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t hsl_lightness;    /*!< Target value of light hsl lightness state */
    uint16_t hsl_hue;          /*!< Target value of light hsl hue state */
    uint16_t hsl_saturation;   /*!< Target value of light hsl saturation state */
    uint8_t  tid;              /*!< Transaction ID */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_hsl_set_t;

/** Parameters of Light HSL Hue Set */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t hue;              /*!< Target value of light hsl hue state */
    uint8_t  tid;              /*!< Transaction ID */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_hsl_hue_set_t;

/** Parameters of Light HSL Saturation Set */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t saturation;       /*!< Target value of light hsl hue state */
    uint8_t  tid;              /*!< Transaction ID */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_hsl_saturation_set_t;

/** Parameters of Light HSL Default Set */
typedef struct {
    uint16_t lightness;        /*!< Value of light lightness default state */
    uint16_t hue;              /*!< Value of light hue default state */
    uint16_t saturation;       /*!< Value of light saturation default state */
} meshx_light_hsl_default_set_t;

/** Parameters of Light HSL Range Set */
typedef struct {
    uint16_t hue_range_min;        /*!< Value of hue range min field of light hsl hue range state */
    uint16_t hue_range_max;        /*!< Value of hue range max field of light hsl hue range state */
    uint16_t saturation_range_min; /*!< Value of saturation range min field of light hsl saturation range state */
    uint16_t saturation_range_max; /*!< Value of saturation range max field of light hsl saturation range state */
} meshx_light_hsl_range_set_t;

/** Parameters of Light xyL Set */
typedef struct {
    bool     op_en;            /*!< Indicate whether optional parameters included */
    uint16_t xyl_lightness;    /*!< The target value of the Light xyL Lightness state */
    uint16_t xyl_x;            /*!< The target value of the Light xyL x state */
    uint16_t xyl_y;            /*!< The target value of the Light xyL y state */
    uint8_t  tid;              /*!< Transaction Identifier */
    uint8_t  trans_time;       /*!< Time to complete state transition (optional) */
    uint8_t  delay;            /*!< Indicate message execution delay (C.1) */
} meshx_light_xyl_set_t;

/** Parameters of Light xyL Default Set */
typedef struct {
    uint16_t lightness;        /*!< The value of the Light Lightness Default state */
    uint16_t xyl_x;            /*!< The value of the Light xyL x Default state */
    uint16_t xyl_y;            /*!< The value of the Light xyL y Default state */
} meshx_light_xyl_default_set_t;

/** Parameters of Light xyL Range Set */
typedef struct {
    uint16_t xyl_x_range_min;  /*!< The value of the xyL x Range Min field of the Light xyL x Range state */
    uint16_t xyl_x_range_max;  /*!< The value of the xyL x Range Max field of the Light xyL x Range state */
    uint16_t xyl_y_range_min;  /*!< The value of the xyL y Range Min field of the Light xyL y Range state */
    uint16_t xyl_y_range_max;  /*!< The value of the xyL y Range Max field of the Light xyL y Range state */
} meshx_light_xyl_range_set_t;

/** Parameter of Light LC Mode Set */
typedef struct {
    uint8_t mode;              /*!< The target value of the Light LC Mode state */
} meshx_light_lc_mode_set_t;

/** Parameter of Light LC OM Set */
typedef struct {
    uint8_t mode;              /*!< The target value of the Light LC Occupancy Mode state */
} meshx_light_lc_om_set_t;

/** Parameters of Light LC Light OnOff Set */
typedef struct {
    bool    op_en;             /*!< Indicate whether optional parameters included */
    uint8_t light_onoff;       /*!< The target value of the Light LC Light OnOff state */
    uint8_t tid;               /*!< Transaction Identifier */
    uint8_t trans_time;        /*!< Time to complete state transition (optional) */
    uint8_t delay;             /*!< Indicate message execution delay (C.1) */
} meshx_light_lc_light_onoff_set_t;

/** Parameter of Light LC Property Get */
typedef struct {
    uint16_t property_id;      /*!< Property ID identifying a Light LC Property */
} meshx_light_lc_property_get_t;

/** Parameters of Light LC Property Set */
typedef struct {
    uint16_t property_id;      /*!< Property ID identifying a Light LC Property */
    struct net_buf_simple *property_value;  /*!< Raw value for the Light LC Property */
} meshx_light_lc_property_set_t;

/**
 * @brief Lighting Client Model get message union
 */
typedef union {
    meshx_light_lc_property_get_t           lc_property_get;             /*!< For MESHX_MODEL_OP_LIGHT_LC_PROPERTY_GET */
} meshx_light_client_get_state_t;

/**
 * @brief Lighting Client Model set message union
 */
typedef union {
    meshx_light_lightness_set_t             lightness_set;               /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET & MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK */
    meshx_light_lightness_linear_set_t      lightness_linear_set;        /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET & MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET_UNACK */
    meshx_light_lightness_default_set_t     lightness_default_set;       /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET & MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET_UNACK */
    meshx_light_lightness_range_set_t       lightness_range_set;         /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET & MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET_UNACK */
    meshx_light_ctl_set_t                   ctl_set;                     /*!< For MESHX_MODEL_OP_LIGHT_CTL_SET & MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK */
    meshx_light_ctl_temperature_set_t       ctl_temperature_set;         /*!< For MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET & MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK */
    meshx_light_ctl_temperature_range_set_t ctl_temperature_range_set;   /*!< For MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET & MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK */
    meshx_light_ctl_default_set_t           ctl_default_set;             /*!< For MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET & MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK */
    meshx_light_hsl_set_t                   hsl_set;                     /*!< For MESHX_MODEL_OP_LIGHT_HSL_SET & MESHX_MODEL_OP_LIGHT_HSL_SET_UNACK */
    meshx_light_hsl_hue_set_t               hsl_hue_set;                 /*!< For MESHX_MODEL_OP_LIGHT_HSL_HUE_SET & MESHX_MODEL_OP_LIGHT_HSL_HUE_SET_UNACK */
    meshx_light_hsl_saturation_set_t        hsl_saturation_set;          /*!< For MESHX_MODEL_OP_LIGHT_HSL_SATURATION_SET & MESHX_MODEL_OP_LIGHT_HSL_SATURATION_SET_UNACK */
    meshx_light_hsl_default_set_t           hsl_default_set;             /*!< For MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_SET & MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_SET_UNACK */
    meshx_light_hsl_range_set_t             hsl_range_set;               /*!< For MESHX_MODEL_OP_LIGHT_HSL_RANGE_SET & MESHX_MODEL_OP_LIGHT_HSL_RANGE_SET_UNACK */
    meshx_light_xyl_set_t                   xyl_set;                     /*!< For MESHX_MODEL_OP_LIGHT_XYL_SET & MESHX_MODEL_OP_LIGHT_XYL_SET_UNACK */
    meshx_light_xyl_default_set_t           xyl_default_set;             /*!< For MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_SET & MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_SET_UNACK */
    meshx_light_xyl_range_set_t             xyl_range_set;               /*!< For MESHX_MODEL_OP_LIGHT_XYL_RANGE_SET & MESHX_MODEL_OP_LIGHT_XYL_RANGE_SET_UNACK */
    meshx_light_lc_mode_set_t               lc_mode_set;                 /*!< For MESHX_MODEL_OP_LIGHT_LC_MODE_SET & MESHX_MODEL_OP_LIGHT_LC_MODE_SET_UNACK */
    meshx_light_lc_om_set_t                 lc_om_set;                   /*!< For MESHX_MODEL_OP_LIGHT_LC_OM_SET & MESHX_MODEL_OP_LIGHT_LC_OM_SET_UNACK */
    meshx_light_lc_light_onoff_set_t        lc_light_onoff_set;          /*!< For MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET & MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET_UNACK */
    meshx_light_lc_property_set_t           lc_property_set;             /*!< For MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET & MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET_UNACK */
} meshx_light_client_set_state_t;

/**
 *  @brief Bluetooth Mesh Light Lightness Client Model Get and Set callback parameters structure.
 */

/** Parameters of Light Lightness Status */
typedef struct {
    bool     op_en;                /*!< Indicate if optional parameters are included */
    uint16_t present_lightness;    /*!< Current value of light lightness actual state */
    uint16_t target_lightness;     /*!< Target value of light lightness actual state (optional) */
    uint8_t  remain_time;          /*!< Time to complete state transition (C.1) */
} meshx_light_lightness_status_cb_t;

/** Parameters of Light Lightness Linear Status */
typedef struct {
    bool     op_en;                /*!< Indicate if optional parameters are included */
    uint16_t present_lightness;    /*!< Current value of light lightness linear state */
    uint16_t target_lightness;     /*!< Target value of light lightness linear state (optional) */
    uint8_t  remain_time;          /*!< Time to complete state transition (C.1) */
} meshx_light_lightness_linear_status_cb_t;

/** Parameter of Light Lightness Last Status */
typedef struct {
    uint16_t lightness;            /*!< The value of the Light Lightness Last state */
} meshx_light_lightness_last_status_cb_t;

/** Parameter of Light Lightness Default Status */
typedef struct {
    uint16_t lightness;            /*!< The value of the Light Lightness default State */
} meshx_light_lightness_default_status_cb_t;

/** Parameters of Light Lightness Range Status */
typedef struct {
    uint8_t  status_code;          /*!< Status Code for the request message */
    uint16_t range_min;            /*!< Value of range min field of light lightness range state */
    uint16_t range_max;            /*!< Value of range max field of light lightness range state */
} meshx_light_lightness_range_status_cb_t;

/** Parameters of Light CTL Status */
typedef struct {
    bool     op_en;                    /*!< Indicate if optional parameters are included */
    uint16_t present_ctl_lightness;    /*!< Current value of light ctl lightness state */
    uint16_t present_ctl_temperature;  /*!< Current value of light ctl temperature state */
    uint16_t target_ctl_lightness;     /*!< Target value of light ctl lightness state (optional) */
    uint16_t target_ctl_temperature;   /*!< Target value of light ctl temperature state (C.1) */
    uint8_t  remain_time;              /*!< Time to complete state transition (C.1) */
} meshx_light_ctl_status_cb_t;

/** Parameters of Light CTL Temperature Status */
typedef struct {
    bool     op_en;                    /*!< Indicate if optional parameters are included */
    uint16_t present_ctl_temperature;  /*!< Current value of light ctl temperature state */
    uint16_t present_ctl_delta_uv;     /*!< Current value of light ctl delta UV state */
    uint16_t target_ctl_temperature;   /*!< Target value of light ctl temperature state (optional) */
    uint16_t target_ctl_delta_uv;      /*!< Target value of light ctl delta UV state (C.1) */
    uint8_t  remain_time;              /*!< Time to complete state transition (C.1) */
} meshx_light_ctl_temperature_status_cb_t;

/** Parameters of Light CTL Temperature Range Status */
typedef struct {
    uint8_t  status_code;      /*!< Status code for the request message */
    uint16_t range_min;        /*!< Value of temperature range min field of light ctl temperature range state */
    uint16_t range_max;        /*!< Value of temperature range max field of light ctl temperature range state */
} meshx_light_ctl_temperature_range_status_cb_t;

/** Parameters of Light CTL Default Status */
typedef struct {
    uint16_t lightness;        /*!< Value of light lightness default state */
    uint16_t temperature;      /*!< Value of light temperature default state */
    int16_t  delta_uv;         /*!< Value of light delta UV default state */
} meshx_light_ctl_default_status_cb_t;

/** Parameters of Light HSL Status */
typedef struct {
    bool     op_en;            /*!< Indicate if optional parameters are included */
    uint16_t hsl_lightness;    /*!< Current value of light hsl lightness state */
    uint16_t hsl_hue;          /*!< Current value of light hsl hue state */
    uint16_t hsl_saturation;   /*!< Current value of light hsl saturation state */
    uint8_t  remain_time;      /*!< Time to complete state transition (optional) */
} meshx_light_hsl_status_cb_t;

/** Parameters of Light HSL Target Status */
typedef struct {
    bool  op_en;                    /*!< Indicate if optional parameters are included */
    uint16_t hsl_lightness_target;     /*!< Target value of light hsl lightness state */
    uint16_t hsl_hue_target;           /*!< Target value of light hsl hue state */
    uint16_t hsl_saturation_target;    /*!< Target value of light hsl saturation state */
    uint8_t  remain_time;              /*!< Time to complete state transition (optional) */
} meshx_light_hsl_target_status_cb_t;

/** Parameters of Light HSL Hue Status */
typedef struct {
    bool     op_en;        /*!< Indicate if optional parameters are included */
    uint16_t present_hue;  /*!< Current value of light hsl hue state */
    uint16_t target_hue;   /*!< Target value of light hsl hue state (optional) */
    uint8_t  remain_time;  /*!< Time to complete state transition (C.1) */
} meshx_light_hsl_hue_status_cb_t;

/** Parameters of Light HSL Saturation Status */
typedef struct {
    bool     op_en;                /*!< Indicate if optional parameters are included */
    uint16_t present_saturation;   /*!< Current value of light hsl saturation state */
    uint16_t target_saturation;    /*!< Target value of light hsl saturation state (optional) */
    uint8_t  remain_time;          /*!< Time to complete state transition (C.1) */
} meshx_light_hsl_saturation_status_cb_t;

/** Parameters of Light HSL Default Status */
typedef struct {
    uint16_t lightness;    /*!< Value of light lightness default state */
    uint16_t hue;          /*!< Value of light hue default state */
    uint16_t saturation;   /*!< Value of light saturation default state */
} meshx_light_hsl_default_status_cb_t;

/** Parameters of Light HSL Range Status */
typedef struct {
    uint8_t  status_code;          /*!< Status code for the request message */
    uint16_t hue_range_min;        /*!< Value of hue range min field of light hsl hue range state */
    uint16_t hue_range_max;        /*!< Value of hue range max field of light hsl hue range state */
    uint16_t saturation_range_min; /*!< Value of saturation range min field of light hsl saturation range state */
    uint16_t saturation_range_max; /*!< Value of saturation range max field of light hsl saturation range state */
} meshx_light_hsl_range_status_cb_t;

/** Parameters of Light xyL Status */
typedef struct {
    bool     op_en;                /*!< Indicate whether optional parameters included */
    uint16_t xyl_lightness;        /*!< The present value of the Light xyL Lightness state */
    uint16_t xyl_x;                /*!< The present value of the Light xyL x state */
    uint16_t xyl_y;                /*!< The present value of the Light xyL y state */
    uint8_t  remain_time;          /*!< Time to complete state transition (optional) */
} meshx_light_xyl_status_cb_t;

/** Parameters of Light xyL Target Status */
typedef struct {
    bool     op_en;                /*!< Indicate whether optional parameters included */
    uint16_t target_xyl_lightness; /*!< The target value of the Light xyL Lightness state */
    uint16_t target_xyl_x;         /*!< The target value of the Light xyL x state */
    uint16_t target_xyl_y;         /*!< The target value of the Light xyL y state */
    uint8_t  remain_time;          /*!< Time to complete state transition (optional) */
} meshx_light_xyl_target_status_cb_t;

/** Parameters of Light xyL Default Status */
typedef struct {
    uint16_t lightness;        /*!< The value of the Light Lightness Default state */
    uint16_t xyl_x;            /*!< The value of the Light xyL x Default state */
    uint16_t xyl_y;            /*!< The value of the Light xyL y Default state */
} meshx_light_xyl_default_status_cb_t;

/** Parameters of Light xyL Range Status */
typedef struct {
    uint8_t  status_code;      /*!< Status Code for the requesting message */
    uint16_t xyl_x_range_min;  /*!< The value of the xyL x Range Min field of the Light xyL x Range state */
    uint16_t xyl_x_range_max;  /*!< The value of the xyL x Range Max field of the Light xyL x Range state */
    uint16_t xyl_y_range_min;  /*!< The value of the xyL y Range Min field of the Light xyL y Range state */
    uint16_t xyl_y_range_max;  /*!< The value of the xyL y Range Max field of the Light xyL y Range state */
} meshx_light_xyl_range_status_cb_t;

/** Parameter of Light LC Mode Status */
typedef struct {
    uint8_t mode;              /*!< The present value of the Light LC Mode state */
} meshx_light_lc_mode_status_cb_t;

/** Parameter of Light LC OM Status */
typedef struct {
    uint8_t mode;              /*!< The present value of the Light LC Occupancy Mode state */
} meshx_light_lc_om_status_cb_t;

/** Parameters of Light LC Light OnOff Status */
typedef struct {
    bool    op_en;                 /*!< Indicate whether optional parameters included */
    uint8_t present_light_onoff;   /*!< The present value of the Light LC Light OnOff state */
    uint8_t target_light_onoff;    /*!< The target value of the Light LC Light OnOff state (Optional) */
    uint8_t remain_time;           /*!< Time to complete state transition (C.1) */
} meshx_light_lc_light_onoff_status_cb_t;

/** Parameters of Light LC Property Status */
typedef struct {
    uint16_t property_id;      /*!< Property ID identifying a Light LC Property */
    struct net_buf_simple *property_value;  /*!< Raw value for the Light LC Property */
} meshx_light_lc_property_status_cb_t;

typedef union
{
    meshx_light_lightness_status_cb_t             lightness_status;             /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_STATUS */
    meshx_light_lightness_linear_status_cb_t      lightness_linear_status;      /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_STATUS */
    meshx_light_lightness_last_status_cb_t        lightness_last_status;        /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_LAST_STATUS */
    meshx_light_lightness_default_status_cb_t     lightness_default_status;     /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_STATUS */
    meshx_light_lightness_range_status_cb_t       lightness_range_status;       /*!< For MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_STATUS */
    meshx_light_ctl_status_cb_t                   ctl_status;                   /*!< For MESHX_MODEL_OP_LIGHT_CTL_STATUS */
    meshx_light_ctl_temperature_status_cb_t       ctl_temperature_status;       /*!< For MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS */
    meshx_light_ctl_temperature_range_status_cb_t ctl_temperature_range_status; /*!< For MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS */
    meshx_light_ctl_default_status_cb_t           ctl_default_status;           /*!< For MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS */
    meshx_light_hsl_status_cb_t                   hsl_status;                   /*!< For MESHX_MODEL_OP_LIGHT_HSL_STATUS */
    meshx_light_hsl_target_status_cb_t            hsl_target_status;            /*!< For MESHX_MODEL_OP_LIGHT_HSL_TARGET_STATUS */
    meshx_light_hsl_hue_status_cb_t               hsl_hue_status;               /*!< For MESHX_MODEL_OP_LIGHT_HSL_HUE_STATUS */
    meshx_light_hsl_saturation_status_cb_t        hsl_saturation_status;        /*!< For MESHX_MODEL_OP_LIGHT_HSL_SATURATION_STATUS */
    meshx_light_hsl_default_status_cb_t           hsl_default_status;           /*!< For MESHX_MODEL_OP_LIGHT_HSL_DEFAULT_STATUS */
    meshx_light_hsl_range_status_cb_t             hsl_range_status;             /*!< For MESHX_MODEL_OP_LIGHT_HSL_RANGE_STATUS */
    meshx_light_xyl_status_cb_t                   xyl_status;                   /*!< For MESHX_MODEL_OP_LIGHT_XYL_STATUS */
    meshx_light_xyl_target_status_cb_t            xyl_target_status;            /*!< For MESHX_MODEL_OP_LIGHT_XYL_TARGET_STATUS */
    meshx_light_xyl_default_status_cb_t           xyl_default_status;           /*!< For MESHX_MODEL_OP_LIGHT_XYL_DEFAULT_STATUS */
    meshx_light_xyl_range_status_cb_t             xyl_range_status;             /*!< For MESHX_MODEL_OP_LIGHT_XYL_RANGE_STATUS */
    meshx_light_lc_mode_status_cb_t               lc_mode_status;               /*!< For MESHX_MODEL_OP_LIGHT_LC_MODE_STATUS */
    meshx_light_lc_om_status_cb_t                 lc_om_status;                 /*!< For MESHX_MODEL_OP_LIGHT_LC_OM_STATUS */
    meshx_light_lc_light_onoff_status_cb_t        lc_light_onoff_status;        /*!< For MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_STATUS */
    meshx_light_lc_property_status_cb_t           lc_property_status;           /*!< For MESHX_MODEL_OP_LIGHT_LC_PROPERTY_STATUS */
} meshx_gen_light_client_status_cb_t;

/**
 * @brief Callback parameters for Generic Light Client Model events.
 *        This structure is used to pass information about the received
 *        messages and their context to the application.
 *        It includes the context, model pointer, event type, and status values.
 *        The status values are encapsulated in a union to handle different
 *        types of status messages that the Generic Light Client Model can receive.
 *        Each status type corresponds to a specific operation code (opcode) defined in the BLE Mesh
 *        specification, allowing the application to handle them appropriately.
 */
typedef struct meshx_gen_light_cli_cb_param
{
    meshx_ctx_t ctx;                            /**< Context of the received messages */
    meshx_model_t model;                        /**< Pointer to Generic Light Client Models */
    meshx_gen_light_cli_evt_t evt;              /**< Event type of the received message */
    meshx_gen_light_client_status_cb_t status;  /**< Value of the received Generic Messages */
} meshx_gen_light_cli_cb_param_t;

/**
 * @brief Initialize the Generic Light Client Model.
 *        This function sets up the necessary parameters and resources
 *        for the Generic Light Client Model to operate correctly.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS on successful initialization,
 *                     or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_gen_light_client_init(void);

/**
 * @brief Creates and initializes a Light CTL (Color Temperature Light) client model instance.
 *
 * This function sets up the Light CTL client model for use in the BLE Mesh network.
 * It associates the client model with the provided model pointer and optionally sets up
 * publication and client context pointers.
 *
 * @param[in]  p_model         Pointer to the mesh model structure to associate with the Light CTL client.
 * @param[out] p_pub           Pointer to a publication context pointer to be initialized (can be NULL if not used).
 * @param[out] p_light_ctl_cli Pointer to a Light CTL client context pointer to be initialized.
 *
 * @return meshx_err_t         Returns MESHX_OK on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_client_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_light_ctl_cli);

/**
 * @brief Deletes the Light client instance and its associated publication context.
 *
 * This function is responsible for cleaning up and freeing resources associated with the Light client model,
 * including its publication context.
 *
 * @param[in] p_pub Pointer to the publication context to be deleted.
 * @param[in] p_cli Pointer to the client instance to be deleted.
 *
 * @return meshx_err_t Returns an error code indicating the result of the delete operation.
 */
meshx_err_t meshx_plat_light_client_delete(meshx_ptr_t* p_pub, meshx_ptr_t* p_cli);

/**
 * @brief Sends a Light Client message over BLE Mesh.
 *
 * This function constructs and sends a Light Client message using the specified model,
 * set state parameters, opcode, destination address, network index, and application index.
 *
 * @param[in] p_model   Pointer to the BLE Mesh model instance.
 * @param[in] p_set     Pointer to the structure containing the light client set state parameters.
 * @param[in] opcode    Opcode of the message to be sent.
 * @param[in] addr      Destination address for the message.
 * @param[in] net_idx   Network index to be used for sending the message.
 * @param[in] app_idx   Application index to be used for sending the message.
 * @param[in] is_get_opcode Indicates whether the opcode is a GET type (true) or SET type (false).
 *
 * @return meshx_err_t  Result of the message send operation.
 */
meshx_err_t meshx_plat_light_client_send_msg(
    meshx_ptr_t p_model, meshx_light_client_set_state_t *p_set,
    uint16_t opcode, uint16_t addr,
    uint16_t net_idx, uint16_t app_idx,
    bool is_get_opcode
);

#endif /* MESHX_BLE_MESH_LIGHT_CLI_H */
