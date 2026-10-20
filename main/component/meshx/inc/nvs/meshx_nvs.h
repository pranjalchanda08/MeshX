/**
 * @file meshx_nvs.h
 * @brief Header file for MeshX Non-Volatile Storage (NVS) operations.
 *
 * This file provides APIs to manage the Non-Volatile Storage (NVS) used in the MeshX system.
 * It includes functions to read, write, erase, and manage key-value pairs stored persistently.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_NVS_H__
#define __MESHX_NVS_H__

#include <stdint.h>
#include <meshx_common.h>
#include "meshx_os_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_NVS_TIMER_PERIOD_DEF  1000

#ifndef MESHX_NVS_TIMER_PERIOD
#define MESHX_NVS_TIMER_PERIOD      MESHX_NVS_TIMER_PERIOD_DEF
#endif /* MESHX_NVS_TIMER_PERIOD */

#define MESHX_NVS_AUTO_COMMIT       true
#define MESHX_NVS_NO_AUTO_COMMIT    false

#ifndef CONFIG_MAX_ELEMENT_COUNT
#define CONFIG_MAX_ELEMENT_COUNT 16
#endif

#define MESHX_NVS_KEY_KR_EPOCH        "kr_epoch"
#define MESHX_NVS_KEY_COMP_SCHEMA     "comp_schema"
#define MESHX_NVS_COMP_MAGIC          0x4D584350  /* 'MXCP' */
#define MESHX_NVS_COMP_VERSION        0x01
#define MESHX_COMP_FLAG_OVERRIDE_RO   0x01

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                                     /**< 0x4D584350 ('MXCP') */
    uint8_t  version;                                   /**< Format version (0x01) */
    uint8_t  element_count;                             /**< Total number of elements */
    uint8_t  flags;                                     /**< Flags (e.g. MESHX_COMP_FLAG_OVERRIDE_RO) */
    uint8_t  reserved;                                  /**< Alignment padding */
    uint16_t element_types[CONFIG_MAX_ELEMENT_COUNT];   /**< Array of meshx_element_type_t */
    uint16_t schema_crc;                                /**< CRC-16-CCITT over preceding fields */
} meshx_nvs_comp_schema_t;

#define MESHX_NVS_KEY_LINK_TABLE      "link_table"
#define MESHX_NVS_LINK_MAGIC          0x4C4E4B53  /* 'LNKS' */
#define MESHX_NVS_LINK_VERSION        0x01
#define MESHX_MAX_PERSISTED_LINKS     16

/**
 * @brief Individual link entry in NVS
 */
typedef struct {
    uint8_t  local_element_idx;
    uint16_t bound_variant;       /**< Element variant type at time of pairing */
    uint16_t target_unicast_addr; /**< Unicast address of peer (if 1:1 link) */
    uint16_t target_group_addr;   /**< Group address (0x0000 if 1:1 unicast) */
    uint16_t func_id;             /**< Function ID bound to this link */
    uint8_t  active;              /**< 1 = active, 0 = free slot */
} meshx_nvs_link_entry_t;

/**
 * @brief Persistent link table header & records
 */
typedef struct {
    uint32_t              magic;       /**< 0x4C4E4B53 ('LNKS') */
    uint8_t               version;     /**< Format version (0x01) */
    uint16_t              link_epoch;  /**< Monotonic counter incremented on every pair/unpair */
    uint8_t               link_count;  /**< Number of active links */
    uint8_t               reserved;
    meshx_nvs_link_entry_t links[MESHX_MAX_PERSISTED_LINKS];
    uint16_t              table_crc;   /**< CRC-16-CCITT across preceding fields */
} meshx_nvs_link_table_t;
#pragma pack(pop)

/**
 * @struct meshx_nvs
 * @brief Structure to hold the MeshX NVS data.
 */
typedef struct meshx_nvs {
    uint16_t init;                          /**< NVS initialization flag */
    uint16_t cid;                           /**< Company ID */
    uint16_t pid;                           /**< Product ID */
    uintptr_t meshx_nvs_handle;             /**< NVS handle */
#ifdef MESHX_NVS_TIMER_PERIOD
    meshx_os_timer_t *meshx_nvs_commit_tmr; /**< NVS stability timer */
#endif /* MESHX_NVS_TIMER_PERIOD */
}meshx_nvs_t;

/**
 * @brief MeshX NVS Initialisation
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_init(void);

/**
 * @brief Erase all key-value pairs stored in the NVS.
 *
 * This function clears all data stored in the Non-Volatile Storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_erase(void);

/**
 * @brief Commit changes to the NVS.
 *
 * This function ensures that any pending changes to the NVS are flushed to persistent storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_commit(void);

/**
 * @brief Close the NVS handle.
 *
 * This function releases any resources associated with the NVS handle.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_close(void);

/**
 * @brief Remove a key-value pair from the NVS.
 *
 * This function deletes a specific key-value pair from the NVS based on the provided key.
 *
 * @param[in] key The key identifying the value to be removed.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_remove(char const* key);

/**
 * @brief Open the NVS with a timeout.
 *
 * This function initializes the NVS and sets a timeout for stability operations.
 * @note NVS Namespace: MESHX_NVS_NAMESPACE
 *
 * @param[in] cid Company ID
 * @param[in] pid Product ID
 * @param[in] commit_timeout_ms Timeout for stability operations in milliseconds.
 *
 *
 * @note commit_timeout_ms = 0 -> use MESHX_NVS_TIMER_PERIOD
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_open(uint16_t cid, uint16_t pid, uint32_t commit_timeout_ms);

/**
 * @brief Get a value from the NVS.
 *
 * This function retrieves a value associated with the given key from the NVS.
 *
 * @param[in]    key         The key identifying the value to be retrieved.
 * @param[out]   blob        Pointer to the buffer where the value will be stored.
 * @param[in]    blob_size   Size of the buffer in bytes.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_get(char const *key, void *blob, uint16_t blob_size);

/**
 * @brief Set a value in the NVS.
 *
 * This function stores a value associated with the given key in the NVS.
 *
 * @param[in] key        The key identifying the value to be stored.
 * @param[in] blob       Pointer to the buffer containing the value.
 * @param[in] blob_size  Size of the buffer in bytes.
 * @param[in] arm_timer  Re-arm stability timer and auto commit
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_set(char const* key, void const* blob, uint16_t blob_size, bool arm_timer);

/**
 * @brief Retrieve the context of a specific element from NVS.
 *
 * @param[in]   element_id   The ID of the element whose context is to be retrieved.
 * @param[in]   element_type The type of the element.
 * @param[out]  blob         Pointer to the buffer where the retrieved context will be stored.
 * @param[in]   blob_size    Size of the buffer provided to store the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully retrieved the context.
 */
meshx_err_t meshx_nvs_element_ctx_get(uint16_t element_id, meshx_element_type_t element_type, void *blob, size_t blob_size);

/**
 * @brief Store the context of a specific element to NVS.
 *
 * @param[in] element_id    The ID of the element whose context is to be stored.
 * @param[in] element_type  The type of the element.
 * @param[in] blob          Pointer to the buffer containing the context to be stored.
 * @param[in] blob_size     Size of the buffer containing the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully stored the context.
 */
meshx_err_t meshx_nvs_element_ctx_set(uint16_t element_id, meshx_element_type_t element_type, const void *blob, size_t blob_size);

/**
 * @brief Remove the context of a specific element from NVS.
 *
 * @param[in] element_id   The ID of the element whose context is to be removed.
 * @param[in] element_type The type of the element (or MESHX_ELEMENT_TYPE_ALL).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully removed the context.
 */
meshx_err_t meshx_nvs_element_ctx_remove(uint16_t element_id, meshx_element_type_t element_type);

/**
 * @brief Store composition schema into NVS.
 *
 * @param[in] schema Pointer to composition schema structure.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_comp_schema_set(const meshx_nvs_comp_schema_t *schema);

/**
 * @brief Retrieve composition schema from NVS with CRC validation.
 *
 * @param[out] schema Pointer to buffer to receive schema.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_comp_schema_get(meshx_nvs_comp_schema_t *schema);

/**
 * @brief Erase composition schema from NVS.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_comp_schema_erase(void);

/**
 * @brief Store link table into NVS with CRC validation.
 *
 * @param[in] table Pointer to link table structure.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_set(const meshx_nvs_link_table_t *table);

/**
 * @brief Retrieve link table from NVS with CRC validation.
 *
 * @param[out] table Pointer to buffer to receive link table.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_get(meshx_nvs_link_table_t *table);

/**
 * @brief Erase link table from NVS.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_erase(void);

/**
 * @brief Get the current Key Rotation epoch counter from NVS.
 *
 * @return Current KR epoch (defaults to 0 if uninitialized).
 */
uint32_t meshx_nvs_get_kr_epoch(void);

/**
 * @brief Set and commit the Key Rotation epoch counter to NVS.
 *
 * @param[in] epoch Monotonically increasing KR epoch.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_set_kr_epoch(uint32_t epoch);

/**
 * @brief Get the current link epoch counter from NVS.
 *
 * @return Current link epoch (defaults to 0 if not yet set or unlinked).
 */
uint16_t meshx_nvs_get_link_epoch(void);

/**
 * @brief Boot-time autonomous self-audit reconciling persisted links against baked elements.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_link_reconcile(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MESHX_NVS_H__ */
