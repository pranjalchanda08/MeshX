# Page 14 — Error Codes Reference

> **[← Decommissioning](./13_decommissioning.md)** | **[← Index](../README.md)** | **[Next: Configuration →](./15_configuration.md)**

---

## 15. Error Codes Reference

### 15.1 General Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| `MESHX_SUCCESS` | `0` | Operation succeeded |
| `MESHX_FAIL` | `1` | Generic failure |
| `MESHX_INVALID_ARG` | `2` | NULL or out-of-range argument |
| `MESHX_ERR_PLAT` | `3` | Platform-level error |
| `MESHX_NO_MEM` | `4` | Heap allocation failed |
| `MESHX_INVALID_STATE` | `5` | Component not in valid state |
| `MESHX_NOT_FOUND` | `6` | Key / resource not found |
| `MESHX_NOT_SUPPORTED` | `7` | Feature not enabled |
| `MESHX_TIMEOUT` | `8` | Operation timed out |
| `MESHX_ERR_NOT_INIT` | `9` | Component not initialized |

### 15.2 RO Config Error Codes (`0x4000` range)

| Code | Value | Meaning |
|------|-------|---------|
| `MESHX_ERR_RO_CFG_FORMAT` | `0x4001` | Partition header magic invalid |
| `MESHX_ERR_RO_CFG_VERSION` | `0x4002` | Incompatible version |
| `MESHX_ERR_RO_CFG_CRC` | `0x4003` | CRC-16 mismatch |

### 15.3 GPIO / HAL Error Codes (`0x5000` range)

| Code | Value | Meaning |
|------|-------|---------|
| `MESHX_ERR_GPIO_INVALID_PIN` | `0x5001` | Logical pin not configured |
| `MESHX_ERR_GPIO_INVALID_MODE` | `0x5002` | Wrong mode for operation |
| `MESHX_ERR_GPIO_PWM_NOT_SUPPORTED` | `0x5006` | Pin not PWM-capable |
| `MESHX_ERR_GPIO_HOSTED_MODE` | `0x500A` | Operation disallowed in hosted mode |
| `MESHX_ERR_GPIO_KV_STORAGE` | `0x500B` | KV engine error persisting GPIO state |

### 15.4 Error Handling Pattern

All MeshX API functions follow the same early-return convention using the `MESHX_ERR_PRINT_RET` macro:

```c
#define MESHX_ERR_PRINT_RET(msg, err)         \
    do {                                        \
        if ((err) != MESHX_SUCCESS) {           \
            MESHX_LOGE(MODULE_ID_COMMON,        \
                "%s (err=0x%x)", (msg), (err)); \
            return (err);                       \
        }                                       \
    } while (0)

// Usage:
err = meshx_nvs_init();
MESHX_ERR_PRINT_RET("MeshX NVS Init failed", err);
```

Non-fatal conditions (e.g., missing NVS state on fresh boot) return `MESHX_SUCCESS` after logging a warning — callers must not fail-fast on expected absent data.

---

> **[← Decommissioning](./13_decommissioning.md)** | **[← Index](../README.md)** | **[Next: Configuration →](./15_configuration.md)**
