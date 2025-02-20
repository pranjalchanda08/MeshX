/**
 * @file meshx_api.c
 * @brief Implementation of the BLE Mesh application API.
 *
 * This file contains the implementation of the BLE Mesh application API.
 * It includes functions to send messages to the BLE Mesh application and
 * register the BLE Mesh application callback.
 *
 * @author Pranjal Chanda
 */
#include <meshx_api.h>
#include <control_task.h>

static struct{

    meshx_app_cb app_cb;
    char msg_buff[MESHX_APP_API_MSG_MAX_SIZE];
}meshx_api_ctrl;

/**
 * @brief Control task handler for BLE Mesh application messages.
 *
 * This function handles BLE Mesh application messages.
 *
 * @param[in] pdev      Pointer to the device structure.
 * @param[in] evt       Event type.
 * @param[in] params    Pointer to the message parameters.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t meshx_el_control_task_handler(const dev_struct_t *pdev, control_task_msg_evt_t evt, void *params)
{
    meshx_app_api_msg_header_t *msg_hdr = (meshx_app_api_msg_header_t *)params;
    void *msg = msg_hdr + sizeof(meshx_app_api_msg_header_t);

    esp_err_t err = ESP_OK;

    if (!pdev || !msg_hdr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (meshx_api_ctrl.app_cb)
    {
        err = meshx_api_ctrl.app_cb(msg_hdr, msg);
    }

    ESP_UNUSED(evt);
    return err;
}

/**
 * @brief Sends a message to the BLE Mesh application.
 *
 * This function sends a message to the BLE Mesh application.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, void *msg)
{
    esp_err_t err = ESP_OK;

    if(!msg || msg_len > MESHX_APP_API_MSG_MAX_SIZE)
        return ESP_ERR_INVALID_ARG;

    meshx_app_api_msg_header_t msg_hdr = {
        .func_id = func_id,
        .msg_len = msg_len,
        .elemment_id = element_id,
        .element_type = element_type,
    };

    memset(meshx_api_ctrl.msg_buff, 0, MESHX_APP_API_MSG_MAX_SIZE);
    memcpy(meshx_api_ctrl.msg_buff, &msg_hdr, sizeof(meshx_app_api_msg_header_t));
    memcpy(meshx_api_ctrl.msg_buff + sizeof(meshx_app_api_msg_header_t), msg, msg_len);

    err = control_task_publish(CONTROL_TASK_MSG_CODE_TO_APP, UINT32_MAX, meshx_api_ctrl.msg_buff, msg_len + sizeof(meshx_app_api_msg_header_t));
    if(err)
    {
        ESP_LOGE(TAG, "Failed to send message to app: (%d)", err);
    }

    return err;
}

/**
 * @brief Registers the BLE Mesh application callback.
 *
 * This function registers the BLE Mesh application callback.
 *
 * @param[in] cb Pointer to the application callback.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_app_reg_element_callback(meshx_app_cb cb)
{
    esp_err_t err = ESP_OK;
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        UINT32_MAX,
        (control_task_msg_handle_t)&meshx_el_control_task_handler);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to register control task callback: (%d)", err);
        return err;
    }

    meshx_api_ctrl.app_cb = cb;

    return err;
}
