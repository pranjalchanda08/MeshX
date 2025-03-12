#ifndef __MESHX_RTOS_UTILS_H__
#define __MESHX_RTOS_UTILS_H__

#include "meshx_err.h"
#include "stddef.h"

meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis);

meshx_err_t meshx_rtos_malloc(void** ptr, size_t size);

meshx_err_t meshx_rtos_free(void** ptr);

size_t meshx_rtos_get_free_heap(void);

#endif /* __MESHX_RTOS_UTILS_H__ */

