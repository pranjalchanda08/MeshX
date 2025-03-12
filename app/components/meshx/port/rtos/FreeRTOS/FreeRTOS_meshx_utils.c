#include "meshx_rtos_utils.h"
#include "freertos/FreeRTOS.h"

meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis)
{
    *millis = xTaskGetTickCount() * (1000 / configTICK_RATE_HZ);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_rtos_malloc(void **ptr, size_t size)
{
    if (!ptr || size == 0)
    {
        return MESHX_INVALID_ARG; // Invalid input
    }

    *ptr = pvPortMalloc(size);
    if (*ptr == NULL)
    {
        return MESHX_NO_MEM; // Memory allocation failed
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_rtos_free(void **ptr)
{
    if (ptr && *ptr)
    {                    // Ensure pointer is valid
        vPortFree(*ptr); // Free the actual allocated memory
        *ptr = NULL;     // Set pointer to NULL to avoid dangling reference
    }
    return MESHX_SUCCESS;
}

size_t meshx_rtos_get_free_heap(void)
{
    return xPortGetFreeHeapSize();
}
