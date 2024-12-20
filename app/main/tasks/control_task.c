#include <control_task.h>

#define TAG CONFIG_CONTROL_TASK_NAME
#define ENUM2STR(_enum) [_enum] = #_enum

static void control_task_handler(const void *args);

static QueueHandle_t control_task_queue;

/* Link list heads per Task msg code type */
static control_task_evt_cb_reg_t * control_task_msg_code_list_heads [CONTROL_TASK_MSG_CODE_MAX];

esp_err_t create_control_task(void)
{
    BaseType_t err;
    err = xTaskCreate(
        (TaskFunction_t)&control_task_handler,
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

esp_err_t control_task_reg_evt_cb(control_task_msg_code_t msg_code, control_task_msg_evt_t evt_bmap, control_task_msg_handle_t cb)
{

    if (cb == NULL
    || evt_bmap == 0
    || msg_code >= CONTROL_TASK_MSG_CODE_MAX)
        return ESP_ERR_INVALID_ARG; // Invalid arguments

    control_task_evt_cb_reg_t **ptr = &control_task_msg_code_list_heads[msg_code];

    while (*ptr != NULL)
        ptr = &(*ptr)->next;

    *ptr = (control_task_evt_cb_reg_t *)malloc(sizeof(control_task_evt_cb_reg_t));
    if (*ptr == NULL)
        return ESP_ERR_NO_MEM; // Memory allocation failed

    (*ptr)->cb = cb;
    (*ptr)->msg_evt_bmap = evt_bmap;
    (*ptr)->next = NULL;

    return ESP_OK;
}

static esp_err_t control_task_msg_dispatch(control_task_msg_code_t msg_code,
    control_task_msg_evt_t evt,
    void* params)
{
    control_task_evt_cb_reg_t *ptr = control_task_msg_code_list_heads[msg_code];
    bool evt_handled = false;

    if (ptr == NULL)
    {
        ESP_LOGE(TAG, "No control task msg callback registered for msg: %p", (void*)msg_code);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "msg_code: %p, evt: %p", (void*) msg_code, (void*) evt);

    while (ptr)
    {
        if ((evt & ptr->msg_evt_bmap) && (ptr->cb != NULL)){
            ptr->cb(evt, params); // Call the registered callback
            evt_handled = true;
        }

        ptr = ptr->next; // Move to the next registration
    }
    if(evt_handled == false)
        ESP_LOGW(TAG, "No handler reg for EVT %p", (void*) evt);

    return ESP_OK;
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

static void control_task_handler(const void *args)
{
    esp_err_t err;
    control_task_msg_t recv_msg;
    ESP_UNUSED(args);
    err = create_control_task_msg_q();
    if (err)
        ESP_LOGE(TAG, "Failed to initialise Control Task Msg Q Err: 0x%x", err);

    while (true)
    {
        if (xQueueReceive(control_task_queue, &recv_msg, portMAX_DELAY) == pdTRUE)
        {
            err = control_task_msg_dispatch(recv_msg.msg_code, recv_msg.msg_evt, recv_msg.msg_evt_params);
            if(err)
                ESP_LOGE(TAG, "Err: 0x%x", err);
            if (recv_msg.msg_evt_params)
            {
                /* If Params were passed Free the allocated memory */
                vPortFree(recv_msg.msg_evt_params);
                ESP_LOGD(TAG, "ESP Heap available: %d", xPortGetFreeHeapSize());
            }
        }
    }
}
