/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_host.c
 * @brief Implementations of the MeshX Host SDK API functions and receive logic.
 */

#include "meshx_host.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Callback pointer for received data path events/messages.
 */
static meshx_api_data_cb_t g_data_cb = NULL;

/**
 * @brief Callback pointer for received control path events/messages.
 */
static meshx_api_ctrl_cb_t g_ctrl_cb = NULL;

/**
 * @brief Initialize the MeshX Host SDK variables.
 *
 * @param[in] rx_data_cb  Callback function pointer for received data path events/messages.
 * @param[in] rx_ctrl_cb  Callback function pointer for received control path events/messages.
 */
void meshx_host_init(meshx_api_data_cb_t rx_data_cb, meshx_api_ctrl_cb_t rx_ctrl_cb)
{
    g_data_cb = rx_data_cb;
    g_ctrl_cb = rx_ctrl_cb;
}

/**
 * @brief Register a callback to handle received data messages.
 *
 * @param[in] cb  Callback function pointer to receive data messages.
 * @return meshx_err_t MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_register_data_cb(meshx_api_data_cb_t cb)
{
    g_data_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Register a callback to handle received control messages.
 *
 * @param[in] cb Callback function pointer to receive control messages.
 * @return meshx_err_t MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_register_ctrl_cb(meshx_api_ctrl_cb_t cb)
{
    g_ctrl_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Send a data path message over the port.
 *
 * @param[in] msg Pointer to the data message structure.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_data_send(const meshx_msg_data_t *msg)
{
    if (!msg) return MESHX_INVALID_ARG;

    // Total size of structure based on header len
    size_t total_len = sizeof(meshx_msg_data_t) + msg->payload_len;

    int ret = meshx_port_tx((const uint8_t*)msg, total_len);
    return (ret == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Send a control path message over the port.
 *
 * @param[in] msg Pointer to the control message structure.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_send(const meshx_msg_ctrl_t *msg)
{
    if (!msg) return MESHX_INVALID_ARG;

    size_t total_len = sizeof(meshx_msg_ctrl_t) + msg->payload_len;

    int ret = meshx_port_tx((const uint8_t*)msg, total_len);
    return (ret == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Parse and process an incoming raw frame, triggering callbacks.
 *
 * @param[in] frame Pointer to the raw frame byte array.
 * @param[in] len   The size of the raw frame.
 */
void meshx_host_process_rx(const uint8_t *frame, size_t len)
{
    if (!frame || len < sizeof(uint16_t)) return;

    uint16_t msg_id = frame[0] | (frame[1] << 8);

    // Assuming 0x10-0x1F is Data Path
    if ((msg_id & 0x7FFF) >= 0x10 && (msg_id & 0x7FFF) <= 0x1F) {
        if (g_data_cb) {
            // Note: Endianness/alignment is assumed to match the host platform.
            // If the host is not Little Endian, byte swapping is required here.
            g_data_cb((const meshx_msg_data_t*)frame);
        }
    } else {
        if (g_ctrl_cb) {
            g_ctrl_cb((const meshx_msg_ctrl_t*)frame);
        }
    }
}

/**
 * @brief Create a control message with a payload and send it.
 *
 * @param[in] msg_id        The message identifier (event or command ID).
 * @param[in] payload       Pointer to the payload bytes to send.
 * @param[in] payload_len   The length of the payload.
 * @return meshx_err_t      MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_send_with_payload(uint16_t msg_id, const void *payload, uint16_t payload_len)
{
    if (payload_len > MESHX_APP_API_MSG_MAX_SIZE) return MESHX_INVALID_ARG;

#ifdef CONFIG_MESHX_HOST_USE_MALLOC
    uint16_t total_len = sizeof(meshx_msg_ctrl_t) + payload_len;
    meshx_msg_ctrl_t *msg = (meshx_msg_ctrl_t*)MESHX_HOST_MALLOC(total_len);
    if (!msg) return MESHX_FAIL;
#else
    uint8_t buffer[sizeof(meshx_msg_ctrl_t) + MESHX_APP_API_MSG_MAX_SIZE];
    meshx_msg_ctrl_t *msg = (meshx_msg_ctrl_t*)buffer;
#endif

    msg->msg_id = msg_id;
    msg->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(msg->payload, payload, payload_len);
    }

    meshx_err_t err = meshx_api_ctrl_send(msg);

#ifdef CONFIG_MESHX_HOST_USE_MALLOC
    MESHX_HOST_FREE(msg);
#endif

    return err;
}

/**
 * @brief Enable or disable hosted mode on the target node.
 *
 * @param[in] enable  1 to enable, 0 to disable.
 * @return meshx_err_t  MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_hosted_mode_enable(uint8_t enable)
{
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_HOSTED_MODE_ENABLE, &enable, sizeof(enable));
}

/**
 * @brief Reset the node and clear its provisioning data.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_node_reset(void)
{
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_NODE_RESET, NULL, 0);
}

/**
 * @brief Request the node's composition data.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_get_composition(void)
{
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GET_COMPOSITION, NULL, 0);
}

/**
 * @brief Request the state of a specific element.
 *
 * @param[in] element_id  The ID of the element to query.
 * @return meshx_err_t  MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_get_element_state(uint16_t element_id)
{
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GET_ELEMENT_STATE, &element_id, sizeof(element_id));
}

/**
 * @brief Set the console routing mode.
 *
 * @param[in] enable  The routing mode to set.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_set_console_routing(uint8_t enable)
{
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_SET_CONSOLE_ROUTING, &enable, sizeof(enable));
}

#ifdef CONFIG_MESHX_ENABLE_GPIO_TEST_API
/**
 * @brief Set the logical level of a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @param[in] level The level to set (0 or 1).
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_set_level(uint8_t pin, uint8_t level)
{
    meshx_msg_gpio_set_level_t payload = { .logical_pin = pin, .level = level };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_SET_LEVEL, &payload, sizeof(payload));
}

/**
 * @brief Request the logical level of a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_get_level(uint8_t pin)
{
    meshx_msg_gpio_get_level_t payload = { .logical_pin = pin };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_GET_LEVEL, &payload, sizeof(payload));
}

/**
 * @brief Toggle the logical level of a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_toggle(uint8_t pin)
{
    meshx_msg_gpio_toggle_t payload = { .logical_pin = pin };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_TOGGLE, &payload, sizeof(payload));
}

/**
 * @brief Set the PWM duty cycle for a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @param[in] duty  The PWM duty cycle value.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_set_pwm_duty(uint8_t pin, uint32_t duty)
{
    meshx_msg_gpio_set_pwm_duty_t payload = { .logical_pin = pin, .duty_cycle = duty };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_DUTY, &payload, sizeof(payload));
}

/**
 * @brief Set the PWM frequency for a GPIO pin.
 *
 * @param[in] pin       The logical pin number.
 * @param[in] freq      The PWM frequency in Hz.
 * @return meshx_err_t  MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_set_pwm_freq(uint8_t pin, uint32_t freq)
{
    meshx_msg_gpio_set_pwm_freq_t payload = { .logical_pin = pin, .frequency = freq };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_FREQ, &payload, sizeof(payload));
}

/**
 * @brief Enable interrupts on a GPIO pin.
 *
 * @param[in] pin       The logical pin number.
 * @param[in] intr_type The interrupt type to configure.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_intr_enable(uint8_t pin, uint8_t intr_type)
{
    meshx_msg_gpio_intr_enable_t payload = { .logical_pin = pin, .enable = intr_type };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_INTR_ENABLE, &payload, sizeof(payload));
}

/**
 * @brief Disable interrupts on a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_intr_disable(uint8_t pin)
{
    meshx_msg_gpio_intr_disable_t payload = { .logical_pin = pin };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_INTR_DISABLE, &payload, sizeof(payload));
}

/**
 * @brief Request the configuration of a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_get_config(uint8_t pin)
{
    meshx_msg_gpio_get_config_t payload = { .logical_pin = pin };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_GET_CONFIG, &payload, sizeof(payload));
}

/**
 * @brief Request the detailed state of a GPIO pin.
 *
 * @param[in] pin   The logical pin number.
 * @return meshx_err_t MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_ctrl_gpio_get_state(uint8_t pin)
{
    meshx_msg_gpio_get_state_t payload = { .logical_pin = pin };
    return meshx_api_ctrl_send_with_payload(MESHX_MSG_CTRL_CMD_GPIO_GET_STATE, &payload, sizeof(payload));
}

#endif /* CONFIG_MESHX_ENABLE_GPIO_TEST_API */

/**
 * @brief Pack and send a data payload to a target element.
 *
 * @param[in] element_id    The target element ID.
 * @param[in] element_type  The target element type.
 * @param[in] func_id       The function identifier for the data command.
 * @param[in] payload       Pointer to the data payload bytes.
 * @param[in] payload_len   The length of the payload.
 * @return meshx_err_t      MESHX_SUCCESS on success, else error code.
 */
meshx_err_t meshx_api_data_send_payload(uint16_t element_id, uint16_t element_type, uint16_t func_id, const void *payload, uint16_t payload_len)
{
    if (payload_len > MESHX_APP_API_MSG_MAX_SIZE) return MESHX_INVALID_ARG;

#ifdef CONFIG_MESHX_HOST_USE_MALLOC
    uint16_t total_len = sizeof(meshx_msg_data_t) + payload_len;
    meshx_msg_data_t *msg = (meshx_msg_data_t*)malloc(total_len);
    if (!msg) return MESHX_FAIL;
#else
    uint8_t buffer[sizeof(meshx_msg_data_t) + MESHX_APP_API_MSG_MAX_SIZE];
    meshx_msg_data_t *msg = (meshx_msg_data_t*)buffer;
#endif

    msg->msg_id = MESHX_MSG_DATA_CMD_SEND;
    msg->element_id = element_id;
    msg->element_type = element_type;
    msg->func_id = func_id;
    msg->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(msg->payload, payload, payload_len);
    }

    meshx_err_t err = meshx_api_data_send(msg);

#ifdef CONFIG_MESHX_HOST_USE_MALLOC
    free(msg);
#endif

    return err;
}

