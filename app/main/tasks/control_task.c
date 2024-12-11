#include <control_task.h>

#define TAG CONFIG_CONTROL_TASK_NAME
#define ENUM2STR(_enum) [_enum] = #_enum

static void control_task_handler(void *args);
static void control_task_msg_code_to_hal_handle(control_task_msg_evt_t evt, void* params);
static void control_task_msg_code_system_handle(control_task_msg_evt_t evt, void* params);
static void control_task_msg_code_to_ble_handle(control_task_msg_evt_t evt, void* params);

static QueueHandle_t control_task_queue;

static control_task_msg_handle_t control_task_msg_handle_table[] =
{
    [CONTROL_TASK_MSG_CODE_TO_HAL] = &control_task_msg_code_to_hal_handle,
    [CONTROL_TASK_MSG_CODE_SYSTEM] = &control_task_msg_code_system_handle,
    [CONTROL_TASK_MSG_CODE_TO_BLE] = &control_task_msg_code_to_ble_handle
};

esp_err_t create_control_task(void)
{
    BaseType_t err;
    err = xTaskCreate(
        &control_task_handler,
        CONFIG_CONTROL_TASK_NAME,
        CONFIG_CONTROL_TASK_STACK_SIZE,
        NULL,
        CONFIG_CONTROL_TASK_PRIO,
        NULL);
    if (err != pdPASS)
        return ESP_FAIL;
    
    return ESP_OK;
}

esp_err_t control_task_send_msg(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const void* msg_evt_params,
                                size_t sizeof_msg_evt_params)
{
    control_task_msg_t send_msg;
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    
    if(sizeof_msg_evt_params != 0){
        send_msg.msg_evt_params = pvPortMalloc(sizeof_msg_evt_params);
        if(!send_msg.msg_evt_params)
            return ESP_ERR_NO_MEM;
        /* Copy the params to allocated space */
        memcpy(send_msg.msg_evt_params, msg_evt_params, sizeof_msg_evt_params);
    }

    send_msg.msg_code = msg_code;
    send_msg.msg_evt = msg_evt;

    xPortInIsrContext() ? xQueueSendFromISR(control_task_queue, &send_msg, &pxHigherPriorityTaskWoken) :
        xQueueSend(control_task_queue, &send_msg, portMAX_DELAY);

    return ESP_OK;
}

static void control_task_msg_code_to_hal_handle(control_task_msg_evt_t evt, void* params)
{
    ESP_LOGD(TAG, "%p, %p", (void*)evt, params);
    for (size_t event_bmap = 0; event_bmap < CONTROL_TASK_MSG_EVT_TO_HAL_MAX; event_bmap++)
    {
        if(evt & (1 << event_bmap))
        {
            switch (1 << event_bmap)
            {
                case CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF");
                    break;
                case CONTROL_TASK_MSG_EVT_TO_HAL_SET_TEMP:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_HAL_SET_TEMP");
                    break;
                case CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS");
                    break;
                default:
                    break;
            }
        }
    }
}

static void control_task_msg_code_system_handle(control_task_msg_evt_t evt, void* params)
{
    ESP_LOGD(TAG, "%p, %p", (void*)evt, params);
}

static void control_task_msg_code_to_ble_handle(control_task_msg_evt_t evt, void* params)
{
    ESP_LOGD(TAG, "%p, %p", (void*)evt, params);
    for (size_t event_bmap = 0; event_bmap < CONTROL_TASK_MSG_EVT_TO_HAL_MAX; event_bmap++)
    {
        if(evt & (1 << event_bmap))
        {
            switch (1 << event_bmap)
            {
                case CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF");
                    break;
                case CONTROL_TASK_MSG_EVT_TO_BLE_SET_TEMP:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_BLE_SET_TEMP");
                    break;
                case CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS:
                    ESP_LOGI(TAG, "CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS");
                    break;
                default:
                    break;
            }
        }
    }
}

static esp_err_t create_control_task_msg_q(void)
{
    control_task_queue = xQueueCreate(CONFIG_CONTROL_TASK_QUEUE_LEN, sizeof(control_task_msg_t));

    if (control_task_queue == NULL)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void control_task_handler(void *args)
{
    esp_err_t err;
    control_task_msg_t recv_msg;
    ESP_UNUSED(args);
    err = create_control_task_msg_q();
    if (err)
    {
        ESP_LOGE(TAG, "Failed to initialise Control Task Msg Q Err: 0x%x", err);
    }

    while (true)
    {
        if (xQueueReceive(control_task_queue, &recv_msg, portMAX_DELAY) == pdTRUE 
            && control_task_msg_handle_table[recv_msg.msg_code])
        {
            control_task_msg_handle_table[recv_msg.msg_code](recv_msg.msg_evt, recv_msg.msg_evt_params);
            if (recv_msg.msg_evt_params)
            {
                /* If Params were passed Free the allocated memory */
                vPortFree(recv_msg.msg_evt_params);
                ESP_LOGD(TAG, "ESP Heap available: %d", xPortGetFreeHeapSize());
            }
        }
    }
}
