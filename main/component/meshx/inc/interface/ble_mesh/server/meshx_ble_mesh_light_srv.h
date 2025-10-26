/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_light_srv.h
 * @brief Header file for the MeshX BLE Mesh Light Server module.
 *        This file defines the data structures, function prototypes, and
 *        callback parameters for managing the Light Server models in a BLE Mesh network.
 *        It includes support for various lighting models such as Light CTL, HSL, xyL, and LC.
 *
 * @details
 * - Defines state change event structures for different lighting models.
 * - Provides APIs for initializing, creating, deleting, and managing Light Server instances.
 * - Supports Light CTL Server state management, including setting and restoring states.
 * - Includes callback parameters for handling received lighting messages.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_BLE_MESH_LIGHT_SRV_H__
#define __MESHX_BLE_MESH_LIGHT_SRV_H__

#include "../meshx_ble_mesh_cmn.h"
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

typedef struct {
    uint16_t lightness_linear;          /*!< The present value of Light Lightness Linear state */
    uint16_t target_lightness_linear;   /*!< The target value of Light Lightness Linear state */

    uint16_t lightness_actual;          /*!< The present value of Light Lightness Actual state */
    uint16_t target_lightness_actual;   /*!< The target value of Light Lightness Actual state */

    uint16_t lightness_last;            /*!< The value of Light Lightness Last state */
    uint16_t lightness_default;         /*!< The value of Light Lightness Default state */

    uint8_t  status_code;               /*!< The status code of setting Light Lightness Range state */
    uint16_t lightness_range_min;       /*!< The minimum value of Light Lightness Range state */
    uint16_t lightness_range_max;       /*!< The maximum value of Light Lightness Range state */
} meshx_light_lightness_state_t;

typedef struct {
    uint16_t lightness;             /*!< The present value of Light CTL Lightness state */
    uint16_t target_lightness;      /*!< The target value of Light CTL Lightness state */

    uint16_t temperature;           /*!< The present value of Light CTL Temperature state */
    uint16_t target_temperature;    /*!< The target value of Light CTL Temperature state */

    int16_t  delta_uv;              /*!< The present value of Light CTL Delta UV state */
    int16_t  target_delta_uv;       /*!< The target value of Light CTL Delta UV state */

    uint8_t  status_code;           /*!< The statue code of setting Light CTL Temperature Range state */
    uint16_t temperature_range_min; /*!< The minimum value of Light CTL Temperature Range state */
    uint16_t temperature_range_max; /*!< The maximum value of Light CTL Temperature Range state */

    uint16_t lightness_default;     /*!< The value of Light Lightness Default state */
    uint16_t temperature_default;   /*!< The value of Light CTL Temperature Default state */
    int16_t  delta_uv_default;      /*!< The value of Light CTL Delta UV Default state */
} meshx_light_ctl_state_t;

typedef struct {
    uint16_t lightness;             /*!< The present value of Light HSL Lightness state */
    uint16_t target_lightness;      /*!< The target value of Light HSL Lightness state */

    uint16_t hue;                   /*!< The present value of Light HSL Hue state */
    uint16_t target_hue;            /*!< The target value of Light HSL Hue state */

    uint16_t saturation;            /*!< The present value of Light HSL Saturation state */
    uint16_t target_saturation;     /*!< The target value of Light HSL Saturation state */

    uint16_t lightness_default;     /*!< The value of Light Lightness Default state */
    uint16_t hue_default;           /*!< The value of Light HSL Hue Default state */
    uint16_t saturation_default;    /*!< The value of Light HSL Saturation Default state */

    uint8_t  status_code;           /*!< The status code of setting Light HSL Hue & Saturation Range state */
    uint16_t hue_range_min;         /*!< The minimum value of Light HSL Hue Range state */
    uint16_t hue_range_max;         /*!< The maximum value of Light HSL Hue Range state */
    uint16_t saturation_range_min;  /*!< The minimum value of Light HSL Saturation state */
    uint16_t saturation_range_max;  /*!< The maximum value of Light HSL Saturation state */
} meshx_light_hsl_state_t;

typedef struct {
    uint16_t lightness;         /*!< The present value of Light xyL Lightness state */
    uint16_t target_lightness;  /*!< The target value of Light xyL Lightness state */

    uint16_t x;                 /*!< The present value of Light xyL x state */
    uint16_t target_x;          /*!< The target value of Light xyL x state */

    uint16_t y;                 /*!< The present value of Light xyL y state */
    uint16_t target_y;          /*!< The target value of Light xyL y state */

    uint16_t lightness_default; /*!< The value of Light Lightness Default state */
    uint16_t x_default;         /*!< The value of Light xyL x Default state */
    uint16_t y_default;         /*!< The value of Light xyL y Default state */

    uint8_t  status_code;       /*!< The status code of setting Light xyL x & y Range state */
    uint16_t x_range_min;       /*!< The minimum value of Light xyL x Range state */
    uint16_t x_range_max;       /*!< The maximum value of Light xyL x Range state */
    uint16_t y_range_min;       /*!< The minimum value of Light xyL y Range state */
    uint16_t y_range_max;       /*!< The maximum value of Light xyL y Range state */
} meshx_light_xyl_state_t;

typedef struct {
    /**
     * 0b0 The controller is turned off.
     * - The binding with the Light Lightness state is disabled.
     * 0b1 The controller is turned on.
     * - The binding with the Light Lightness state is enabled.
     */
    uint32_t mode : 1;                  /*!< The value of Light LC Mode state */
    uint32_t occupancy_mode : 1;        /*!< The value of Light LC Occupancy Mode state */
    uint32_t light_onoff : 1;           /*!< The present value of Light LC Light OnOff state */
    uint32_t target_light_onoff : 1;    /*!< The target value of Light LC Light OnOff state */
    uint32_t occupancy : 1;             /*!< The value of Light LC Occupancy state */
    uint32_t ambient_luxlevel : 24;     /*!< The value of Light LC Ambient LuxLevel state */

    /**
     * 1. Light LC Linear Output = max((Lightness Out)^2/65535, Regulator Output)
     * 2. If the Light LC Mode state is set to 0b1, the binding is enabled and upon
     *    a change of the Light LC Linear Output state, the following operation
     *    shall be performed:
     *    Light Lightness Linear = Light LC Linear Output
     * 3. If the Light LC Mode state is set to 0b0, the binding is disabled (i.e.,
     *    upon a change of the Light LC Linear Output state, no operation on the
     *    Light Lightness Linear state is performed).
     */
    uint16_t linear_output;     /*!< The value of Light LC Linear Output state */
} meshx_light_lc_state_t;

/**
 * @brief Lighting Server Model state
 *        Used to set/restore the model state values internally
 */
typedef struct meshx_lighting_server_state
{
    meshx_light_lightness_state_t lightness;      /*!< Light Lightness state */
    meshx_light_ctl_state_t ctl;                  /*!< Light CTL state */
    meshx_light_hsl_state_t hsl;                  /*!< Light HSL state */
    meshx_light_xyl_state_t xyl;                  /*!< Light xyL state */
    meshx_light_lc_state_t lc;                    /*!< Light LC state */
}meshx_lighting_server_state_t;

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

/**
 * @brief Structure representing the Light CTL Server model.
 *
 * This structure contains the model and state information for the Light CTL Server.
 */
typedef struct meshx_light_ctl_srv
{
    meshx_model_t model;                /**< Light CTL Server model */
    meshx_light_ctl_srv_state_t state;  /**< State of the Light CTL Server */
} meshx_light_ctl_srv_t;

/**
 * @brief Initialize the platform-specific Light Server.
 *
 * This function sets up the necessary resources for the Light Server.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_srv_init(void);

/**
 * @brief Restore the Light Server model state from persistent storage.
 *
 * This function restores the Light Server model state from persistent storage.
 *
 * @param[in] p_model    Pointer to the model instance to be restored.
 * @param[in] state      Pointer to the state data to be restored.
 * @param[in] state_len  Length of the state data in bytes.
 *
 * @return meshx_err_t Returns an error code indicating success or failure of the operation.
 *         - MESHX_SUCCESS on success.
 *         - MESHX_ERR_INVALID_ARG if any parameter is invalid.
 *         - MESHX_ERR_INVALID_STATE if the state data is corrupted.
 *         - Other error codes for implementation-specific failures.
 */
meshx_err_t meshx_plat_light_srv_restore(meshx_ptr_t p_model, const meshx_lighting_server_state_t *state, uint16_t state_len);

/**
 * @brief Creates and initializes a Light CTL (Color Temperature Lightness) Setup Server model instance.
 *
 * This function sets up the Light CTL Setup Server for a given model, configuring publication and server context pointers.
 *
 * @param[in]  p_model    Pointer to the parent model instance.
 * @param[out] p_pub      Pointer to the publication context to be initialized.
 * @param[out] p_ctl_srv  Pointer to the Light CTL Setup Server context to be initialized.
 *
 * @return meshx_err_t    Error code indicating the result of the operation.
 *                       - MESHX_OK on success
 *                       - Appropriate error code otherwise
 */
meshx_err_t meshx_plat_light_ctl_setup_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv);

/**
 * @brief Create a Light CTL Server instance.
 *
 * This function initializes and allocates resources for a Light CTL Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_ctl_srv Pointer to the Light CTL Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv);

/**
 * @brief Delete a Light CTL Server instance.
 *
 * This function releases resources associated with a Light CTL Server model.
 *
 * @param[in,out] p_pub    Pointer to the publication context to be deleted.
 * @param[in,out] p_ctl_srv Pointer to the Light CTL Server instance to be deleted.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_srv_delete(meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv);

/**
 * @brief Set the state of the Light CTL Server.
 *
 * This function updates the state of the Light CTL Server with the provided parameters.
 *
 * @param[in] p_model         Pointer to the model instance.
 * @param[in] delta_uv        Delta UV value.
 * @param[in] lightness       Lightness value.
 * @param[in] temperature     Temperature value.
 * @param[in] temp_range_max  Maximum temperature range.
 * @param[in] temp_range_min  Minimum temperature range.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_set_light_ctl_srv_state(meshx_ptr_t p_model,
                                               uint16_t delta_uv,
                                               uint16_t lightness,
                                               uint16_t temperature,
                                               uint16_t temp_range_max,
                                               uint16_t temp_range_min);

/**
 * @brief Restore the state of the Light CTL Server.
 *
 * This function restores the state of the Light CTL Server with the provided parameters.
 *
 * @param[in] p_model         Pointer to the model instance.
 * @param[in] delta_uv        Delta UV value.
 * @param[in] lightness       Lightness value.
 * @param[in] temperature     Temperature value.
 * @param[in] temp_range_max  Maximum temperature range.
 * @param[in] temp_range_min  Minimum temperature range.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_srv_restore(meshx_ptr_t p_model,
                                             uint16_t delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature,
                                             uint16_t temp_range_max,
                                             uint16_t temp_range_min);

/**
 * @brief Send a status message from the Light Server.
 * This function constructs and sends a status message containing the current state of the Light Server.
 * @param[in] p_model       Pointer to the Light Server model.
 * @param[in] p_ctx         Pointer to the context containing message information.
 * @param[in] state_change  The state change data to be sent in the status message.
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
meshx_err_t meshx_plat_gen_light_srv_send_status(const meshx_model_t *p_model,
                                                 const meshx_ctx_t *p_ctx,
                                                 const meshx_lighting_server_state_change_t *state_change);

#endif /* __MESHX_BLE_MESH_LIGHT_SRV_H__ */
