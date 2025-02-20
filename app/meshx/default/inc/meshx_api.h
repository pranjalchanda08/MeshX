#ifndef __MESHX_API_H__
#define __MESHX_API_H__

#include <app_common.h>

#define MESHX_APP_API_MSG_MAX_SIZE  (sizeof(meshx_app_api_msg_header_t) + 255)

/**
 * @brief Structure for the BLE Mesh application API message header.
 *
 * This structure defines the header for BLE Mesh application API messages.
 *
 */
typedef struct meshx_app_api_msg_header{
    uint16_t elemment_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t msg_len;
}meshx_app_api_msg_header_t;

typedef esp_err_t (*meshx_app_cb)(meshx_app_api_msg_header_t *msg_hdr, void *msg);

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
esp_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, void *msg);

/**
 * @brief Registers the BLE Mesh application callback.
 *
 * This function registers the BLE Mesh application callback.
 *
 * @param[in] cb Pointer to the application callback.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_app_reg_element_callback(meshx_app_cb cb);

#endif // __MESHX_API_H__
