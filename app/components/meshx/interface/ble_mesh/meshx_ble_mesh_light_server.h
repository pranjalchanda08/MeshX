#ifndef __MESHX_BLE_MESH_LIGHT_SRV_H__
#define __MESHX_BLE_MESH_LIGHT_SRV_H__

#include "meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

/** Parameter of Light Lightness Actual state change event */
typedef struct
{
    uint16_t lightness; /*!< The value of Light Lightness Actual state */
} meshx_state_change_light_lightness_set_t;

/** Parameter of Light Lightness Linear state change event */
typedef struct
{
    uint16_t lightness; /*!< The value of Light Lightness Linear state */
} meshx_state_change_light_lightness_linear_set_t;

/** Parameter of Light Lightness Default state change event */
typedef struct
{
    uint16_t lightness; /*!< The value of Light Lightness Default state */
} meshx_state_change_light_lightness_default_set_t;

/** Parameters of Light Lightness Range state change event */
typedef struct
{
    uint16_t range_min; /*!< The minimum value of Light Lightness Range state */
    uint16_t range_max; /*!< The maximum value of Light Lightness Range state */
} meshx_state_change_light_lightness_range_set_t;

/** Parameters of Light CTL state change event */
typedef struct
{
    uint16_t lightness;   /*!< The value of Light CTL Lightness state */
    uint16_t temperature; /*!< The value of Light CTL Temperature state */
    int16_t delta_uv;     /*!< The value of Light CTL Delta UV state */
} meshx_state_change_light_ctl_set_t;

/** Parameters of Light CTL Temperature state change event */
typedef struct
{
    uint16_t temperature; /*!< The value of Light CTL Temperature state */
    int16_t delta_uv;     /*!< The value of Light CTL Delta UV state */
} meshx_state_change_light_ctl_temperature_set_t;

/** Parameters of Light CTL Temperature Range state change event */
typedef struct
{
    uint16_t range_min; /*!< The minimum value of Light CTL Temperature Range state */
    uint16_t range_max; /*!< The maximum value of Light CTL Temperature Range state */
} meshx_state_change_light_ctl_temperature_range_set_t;

/** Parameters of Light CTL Default state change event */
typedef struct
{
    uint16_t lightness;   /*!< The value of Light Lightness Default state */
    uint16_t temperature; /*!< The value of Light CTL Temperature Default state */
    int16_t delta_uv;     /*!< The value of Light CTL Delta UV Default state */
} meshx_state_change_light_ctl_default_set_t;

/** Parameters of Light HSL state change event */
typedef struct
{
    uint16_t lightness;  /*!< The value of Light HSL Lightness state */
    uint16_t hue;        /*!< The value of Light HSL Hue state */
    uint16_t saturation; /*!< The value of Light HSL Saturation state */
} meshx_state_change_light_hsl_set_t;

/** Parameter of Light HSL Hue state change event */
typedef struct
{
    uint16_t hue; /*!< The value of Light HSL Hue state */
} meshx_state_change_light_hsl_hue_set_t;

/** Parameter of Light HSL Saturation state change event */
typedef struct
{
    uint16_t saturation; /*!< The value of Light HSL Saturation state */
} meshx_state_change_light_hsl_saturation_set_t;

/** Parameters of Light HSL Default state change event */
typedef struct
{
    uint16_t lightness;  /*!< The value of Light HSL Lightness Default state */
    uint16_t hue;        /*!< The value of Light HSL Hue Default state */
    uint16_t saturation; /*!< The value of Light HSL Saturation Default state */
} meshx_state_change_light_hsl_default_set_t;

/** Parameters of Light HSL Range state change event */
typedef struct
{
    uint16_t hue_range_min;        /*!< The minimum hue value of Light HSL Range state */
    uint16_t hue_range_max;        /*!< The maximum hue value of Light HSL Range state */
    uint16_t saturation_range_min; /*!< The minimum saturation value of Light HSL Range state */
    uint16_t saturation_range_max; /*!< The maximum saturation value of Light HSL Range state */
} meshx_state_change_light_hsl_range_set_t;

/** Parameters of Light xyL state change event */
typedef struct
{
    uint16_t lightness; /*!< The value of Light xyL Lightness state */
    uint16_t x;         /*!< The value of Light xyL x state */
    uint16_t y;         /*!< The value of Light xyL y state */
} meshx_state_change_light_xyl_set_t;

/** Parameters of Light xyL Default state change event */
typedef struct
{
    uint16_t lightness; /*!< The value of Light Lightness Default state */
    uint16_t x;         /*!< The value of Light xyL x Default state */
    uint16_t y;         /*!< The value of Light xyL y Default state */
} meshx_state_change_light_xyl_default_set_t;

/** Parameters of Light xyL Range state change event */
typedef struct
{
    uint16_t x_range_min; /*!< The minimum value of Light xyL x Range state */
    uint16_t x_range_max; /*!< The maximum value of Light xyL x Range state */
    uint16_t y_range_min; /*!< The minimum value of Light xyL y Range state */
    uint16_t y_range_max; /*!< The maximum value of Light xyL y Range state */
} meshx_state_change_light_xyl_range_set_t;

/** Parameter of Light LC Mode state change event */
typedef struct
{
    uint8_t mode; /*!< The value of Light LC Mode state */
} meshx_state_change_light_lc_mode_set_t;

/** Parameter of Light LC Occupancy Mode state change event */
typedef struct
{
    uint8_t mode; /*!< The value of Light LC Occupancy Mode state */
} meshx_state_change_light_lc_om_set_t;

/** Parameter of Light LC Light OnOff state change event */
typedef struct
{
    uint8_t onoff; /*!< The value of Light LC Light OnOff state */
} meshx_state_change_light_lc_light_onoff_set_t;

/** Parameters of Light LC Property state change event */
typedef struct
{
    uint16_t property_id;                  /*!< The property id of Light LC Property state */
    struct net_buf_simple *property_value; /*!< The property value of Light LC Property state */
} meshx_state_change_light_lc_property_set_t;

/** Parameters of Sensor Status state change event */
typedef struct
{
    uint16_t property_id; /*!< The value of Sensor Property ID */
    /** Parameters of Sensor Status related state */
    union
    {
        uint8_t occupancy;                 /*!< The value of Light LC Occupancy state */
        uint32_t set_occupancy_to_1_delay; /*!< The value of Light LC Set Occupancy to 1 Delay state */
        uint32_t ambient_luxlevel;         /*!< The value of Light LC Ambient Luxlevel state */
    } state;
} meshx_state_change_sensor_status_t;

/**
 * @brief Lighting Server Model state change value union
 */
typedef union
{
    /**
     * The recv_op in ctx can be used to decide which state is changed.
     */
    meshx_state_change_light_lightness_set_t lightness_set;                  /*!< Light Lightness Set */
    meshx_state_change_light_lightness_linear_set_t lightness_linear_set;    /*!< Light Lightness Linear Set */
    meshx_state_change_light_lightness_default_set_t lightness_default_set;  /*!< Light Lightness Default Set */
    meshx_state_change_light_lightness_range_set_t lightness_range_set;      /*!< Light Lightness Range Set */
    meshx_state_change_light_ctl_set_t ctl_set;                              /*!< Light CTL Set */
    meshx_state_change_light_ctl_temperature_set_t ctl_temp_set;             /*!< Light CTL Temperature Set */
    meshx_state_change_light_ctl_temperature_range_set_t ctl_temp_range_set; /*!< Light CTL Temperature Range Set */
    meshx_state_change_light_ctl_default_set_t ctl_default_set;              /*!< Light CTL Default Set */
    meshx_state_change_light_hsl_set_t hsl_set;                              /*!< Light HSL Set */
    meshx_state_change_light_hsl_hue_set_t hsl_hue_set;                      /*!< Light HSL Hue Set */
    meshx_state_change_light_hsl_saturation_set_t hsl_saturation_set;        /*!< Light HSL Saturation Set */
    meshx_state_change_light_hsl_default_set_t hsl_default_set;              /*!< Light HSL Default Set */
    meshx_state_change_light_hsl_range_set_t hsl_range_set;                  /*!< Light HSL Range Set */
    meshx_state_change_light_xyl_set_t xyl_set;                              /*!< Light xyL Set */
    meshx_state_change_light_xyl_default_set_t xyl_default_set;              /*!< Light xyL Default Set */
    meshx_state_change_light_xyl_range_set_t xyl_range_set;                  /*!< Light xyL Range Set */
    meshx_state_change_light_lc_mode_set_t lc_mode_set;                      /*!< Light LC Mode Set */
    meshx_state_change_light_lc_om_set_t lc_om_set;                          /*!< Light LC Occupancy Mode Set */
    meshx_state_change_light_lc_light_onoff_set_t lc_light_onoff_set;        /*!< Light LC Light OnOff Set */
    meshx_state_change_light_lc_property_set_t lc_property_set;              /*!< Light LC Property Set */
    meshx_state_change_sensor_status_t sensor_status;                        /*!< Sensor Status */
} meshx_lighting_server_state_change_t;

/** Lighting Server Model callback parameters */
typedef struct meshx_lighting_server_cb_param
{
    meshx_ctx_t ctx;                                   /*!< Context of the received messages */
    meshx_model_t model;                               /*!< Pointer to Lighting Server Models */
    meshx_lighting_server_state_change_t state_change; /*!< Value of the received Lighting Messages */
} meshx_lighting_server_cb_param_t;

typedef struct meshx_light_ctl_srv_state
{
    uint16_t lightness;        /*!< The present value of Light CTL Lightness state */
    uint16_t target_lightness; /*!< The target value of Light CTL Lightness state */

    uint16_t temperature;        /*!< The present value of Light CTL Temperature state */
    uint16_t target_temperature; /*!< The target value of Light CTL Temperature state */

    int16_t delta_uv;        /*!< The present value of Light CTL Delta UV state */
    int16_t target_delta_uv; /*!< The target value of Light CTL Delta UV state */

    uint8_t status_code;            /*!< The statue code of setting Light CTL Temperature Range state */
    uint16_t temperature_range_min; /*!< The minimum value of Light CTL Temperature Range state */
    uint16_t temperature_range_max; /*!< The maximum value of Light CTL Temperature Range state */

    uint16_t lightness_default;   /*!< The value of Light Lightness Default state */
    uint16_t temperature_default; /*!< The value of Light CTL Temperature Default state */
    int16_t delta_uv_default;     /*!< The value of Light CTL Delta UV Default state */
} meshx_light_ctl_srv_state_t;

typedef struct meshx_light_ctl_srv
{
    meshx_model_t model;
    meshx_light_ctl_srv_state_t state;
} meshx_light_ctl_srv_t;

meshx_err_t meshx_plat_light_srv_init(void);

meshx_err_t meshx_plat_light_ctl_srv_create(void **p_model, void **p_pub, void **p_ctl_srv);

meshx_err_t meshx_plat_light_ctl_srv_delete(void **p_model, void **p_pub, void **p_ctl_srv);

meshx_err_t meshx_plat_set_light_ctl_srv_state(void *p_model,
                                               uint16_t delta_uv,
                                               uint16_t lightness,
                                               uint16_t temperature,
                                               uint16_t temp_range_max,
                                               uint16_t temp_range_min);

meshx_err_t meshx_plat_light_ctl_srv_restore(void *p_model,
                                             uint16_t delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature,
                                             uint16_t temp_range_max,
                                             uint16_t temp_range_min);

#endif /* __MESHX_BLE_MESH_LIGHT_SRV_H__ */
