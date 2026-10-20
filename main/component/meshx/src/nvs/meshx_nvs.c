/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_nvs.c
 * @brief Implementation for MeshX Non-Volatile Storage (NVS) operations.
 *
 * This file provides APIs to manage the Non-Volatile Storage (NVS) used in the MeshX system.
 * It includes functions to read, write, erase, and manage key-value pairs stored persistently.
 *
 * @author Pranjal Chanda
 *
 */
#include "meshx_nvs.h"
#include "unit_test.h"
#include "interface/utils/meshx_nvs_interface.h"
#include "meshx_control_task.h"
#include "interface/utils/meshx_tiny_printf.h"
#include "meshx_group_allocator.h"
#include "meshx_compat_matrix.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include <stddef.h>

#define MESHX_NVS_INIT_MAGIC        0x5489

#ifdef CONFIG_BLE_MESH_SPECIFIC_PARTITION
#define MESHX_NVS_PARTITION         CONFIG_BLE_MESH_PARTITION_NAME
#endif

#define MESHX_NVS_NAMESPACE_PID     "MESHX_PID"
#define MESHX_NVS_NAMESPACE_CID     "MESHX_CID"
#define MESHX_NVS_TIMER_NAME        "MESHX_COMMIT_TIMER"
#define MESHX_NVS_ELEMENT_CTX       "MXE_%02x_%04x"
#define MESHX_NVS_RELOAD_ONE_SHOT   0
#define MESHX_KEY_NAME_MAX_SIZE     16
#define MESHX_KEY_VALUE_MAX_SIZE    256

/*
 * @struct meshx_nvs_write_list
 * @brief Linked list structure used to store key-value pairs that need to be written to NVS.
 */
typedef struct meshx_nvs_write_list
{
    uint16_t  len;                            /*< Length of the value in bytes */
    char      key [MESHX_KEY_NAME_MAX_SIZE];  /*< Key name. Maximum length is (MESHX_KEY_NAME_MAX_SIZE-1) characters */
    uint8_t   data[MESHX_KEY_VALUE_MAX_SIZE]; /*< Buffer containing the value to be written to NVS */
    struct meshx_nvs_write_list *next;        /*< Pointer to the next node in the list */
}meshx_nvs_write_list_t;

/*
 * @var meshx_nvs_write_list_head
 * @brief Head of the linked list used to store key-value pairs that need to be written to NVS.
 */
meshx_nvs_write_list_t *meshx_nvs_write_list_head = NULL;

#if CONFIG_ENABLE_UNIT_TEST
#define MESHX_NVS_UNIT_TEST_KEY     "MESHX_UT"

static meshx_err_t meshx_nvs_unit_test_cb_handler(int cmd_id, int argc, char **argv);
#endif

static meshx_nvs_t meshx_nvs_inst;


static meshx_err_t meshx_nvs_control_msg_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    if (evt & CONTROL_TASK_MSG_EVT_SYSTEM_NVS_COMMIT)
    {
        meshx_nvs_commit();
    }
    return MESHX_SUCCESS;
}

#if MESHX_NVS_TIMER_PERIOD
/*
 * @brief MeshX NVS Timer callback.
 * @note This is responsible for performing the commit after stability timeout
 *
 * @param[in]   p_timer     meshx_os_timer param pointer *
 */
static void meshx_nvs_os_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "%s fire -> Offloading commit to control task", OS_TMER_GET_TIMER_NAME(p_timer));

    control_task_msg_publish(CONTROL_TASK_MSG_CODE_SYSTEM, CONTROL_TASK_MSG_EVT_SYSTEM_NVS_COMMIT, NULL, 0);
}
#endif /* MESHX_NVS_TIMER_PERIOD */

/*
 * @brief Erase the NVS and set the product ID.
 *
 * This function erases the NVS and sets the product ID.
 *
 * @param[in] cid Company ID
 * @param[in] pid Product ID
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
static meshx_err_t meshx_nvs_erase_prod_init(uint16_t cid, uint16_t pid)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_nvs_erase();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "meshx_nvs_erase 0x%x", (uint32_t)err);
        return err;
    }

    meshx_nvs_inst.pid = pid;
    meshx_nvs_inst.cid = cid;

    err = meshx_nvs_set(
        MESHX_NVS_NAMESPACE_PID,
        &(meshx_nvs_inst.pid),
        sizeof(meshx_nvs_inst.pid),
        MESHX_NVS_NO_AUTO_COMMIT);

    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "meshx_nvs_set 0x%x", (uint32_t)err);
        return err;
    }

    err = meshx_nvs_set(
        MESHX_NVS_NAMESPACE_CID,
        &(meshx_nvs_inst.cid),
        sizeof(meshx_nvs_inst.cid),
        MESHX_NVS_AUTO_COMMIT);

    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "meshx_nvs_set 0x%x", (uint32_t)err);
        return err;
    }

    meshx_nvs_commit();

    return err;
}

/*
 * @brief MeshX NVS Initialisation
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MODULE_ID_COMPONENT_MESHX_NVS, &meshx_nvs_unit_test_cb_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */

    err = control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_SYSTEM, CONTROL_TASK_MSG_EVT_SYSTEM_NVS_COMMIT, &meshx_nvs_control_msg_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "control_task_msg_subscribe failed: (%d)", err);
        return err;
    }

    return err;
}

/*
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
meshx_err_t meshx_nvs_open(uint16_t cid, uint16_t pid, uint32_t commit_timeout_ms)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (meshx_nvs_inst.init == MESHX_NVS_INIT_MAGIC)
    {
        return MESHX_INVALID_STATE;
    }

    err = meshx_nvs_plat_open(&(meshx_nvs_inst.meshx_nvs_handle));
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "nvs_open 0x%x", (uint32_t)err);
        return err;
    }

#if MESHX_NVS_TIMER_PERIOD
    if(commit_timeout_ms == 0)
    {
        commit_timeout_ms = MESHX_NVS_TIMER_PERIOD;
    }
    err = meshx_os_timer_create(
        MESHX_NVS_TIMER_NAME,
        commit_timeout_ms,
        MESHX_NVS_RELOAD_ONE_SHOT,
        &meshx_nvs_os_timer_cb,
        &(meshx_nvs_inst.meshx_nvs_commit_tmr));
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "os_timer_create 0x%x", (uint32_t)err);
        return err;
    }
#else
    MESHX_UNUSED(commit_timeout_ms);
#endif /* MESHX_NVS_TIMER_PERIOD */

    meshx_nvs_inst.init = MESHX_NVS_INIT_MAGIC;

    err = meshx_nvs_get(
        MESHX_NVS_NAMESPACE_CID,
            &(meshx_nvs_inst.cid),
        sizeof(meshx_nvs_inst.cid));

    if (err == MESHX_SUCCESS)
    {
        err = meshx_nvs_get(
            MESHX_NVS_NAMESPACE_PID,
                &(meshx_nvs_inst.pid),
            sizeof(meshx_nvs_inst.pid));
    }

    if(err != MESHX_SUCCESS)
    {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_NVS, "Product ID not found in NVS reinitializing MeshX NVS (err: 0x%x)", err);
        err = meshx_nvs_erase_prod_init(cid, pid);
    }

    else
    {
        if (meshx_nvs_inst.cid == cid && meshx_nvs_inst.pid == pid)
        {
            MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "Product ID match: %x|%x", meshx_nvs_inst.pid, meshx_nvs_inst.cid);
        }
        else
        {
            MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_NVS, "Product ID mismatch: %x|%x (Expected: %x|%x)", meshx_nvs_inst.pid, meshx_nvs_inst.cid, pid, cid);
            err = meshx_nvs_erase_prod_init(cid, pid);
        }
    }

    return err;
}

/*
 * @brief Erase all key-value pairs stored in the NVS.
 *
 * This function clears all data stored in the Non-Volatile Storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_erase(void)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    return meshx_nvs_plat_erase(meshx_nvs_inst.meshx_nvs_handle);
}

/*
 * @brief Commit changes to the NVS.
 *
 * This function ensures that any pending changes to the NVS are flushed to persistent storage.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_commit(void)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    meshx_err_t err = MESHX_SUCCESS;
    meshx_nvs_write_list_t *cur = meshx_nvs_write_list_head;
    meshx_nvs_write_list_t *prev;

    if (!cur) return MESHX_SUCCESS;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "Committing pending NVS writes...");

    while (cur) {
        err = meshx_nvs_plat_write(meshx_nvs_inst.meshx_nvs_handle, cur->key, cur->data, cur->len);
        if (err) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "commit failed for key: %s", cur->key);
        }
        prev = cur;
        cur = cur->next;
        MESHX_FREE(&prev);
    }

    meshx_nvs_write_list_head = NULL;

    if (err == MESHX_SUCCESS) {
        // Only commit to platform once, after all writes
        err = meshx_nvs_plat_commit(meshx_nvs_inst.meshx_nvs_handle);
    }

    return err;
}


/*
 * @brief Close the NVS handle.
 *
 * This function releases any resources associated with the NVS handle.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_close(void)
{
    meshx_err_t err = MESHX_SUCCESS;
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    err = meshx_nvs_plat_close(meshx_nvs_inst.meshx_nvs_handle);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "nvs_close 0x%x", (uint32_t)err);
    }

#if MESHX_NVS_TIMER_PERIOD
    err = meshx_os_timer_delete(&(meshx_nvs_inst.meshx_nvs_commit_tmr));
#endif /* MESHX_NVS_TIMER_PERIOD */
    meshx_nvs_inst.init = 0;
    return err;
}

/*
 * @brief Remove a key-value pair from the NVS.
 *
 * This function deletes a specific key-value pair from the NVS based on the provided key.
 *
 * @param[in] key The key identifying the value to be removed.
 *
 * @return
 *  - MESHX_SUCCESS: Success.
 */
meshx_err_t meshx_nvs_remove(char const *key)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    return meshx_nvs_plat_remove(meshx_nvs_inst.meshx_nvs_handle, key);
}

/*
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
meshx_err_t meshx_nvs_get(char const *key, void *blob, uint16_t blob_size)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;
    return meshx_nvs_plat_read(meshx_nvs_inst.meshx_nvs_handle, key, blob, blob_size);
}

/*
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
meshx_err_t meshx_nvs_set(char const* key, void const* blob, uint16_t blob_size, bool arm_timer)
{
    meshx_nvs_write_list_t *cur = NULL;
    meshx_nvs_write_list_t *node = NULL;
#if MESHX_NVS_TIMER_PERIOD
    meshx_err_t timer_err = MESHX_SUCCESS;
#endif

    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    if (blob_size > MESHX_KEY_VALUE_MAX_SIZE)
        return MESHX_INVALID_ARG;

    // Check if key already exists in list → overwrite instead of duplicate
    cur = meshx_nvs_write_list_head;
    while (cur) {
        if (strncmp(cur->key, key, MESHX_KEY_NAME_MAX_SIZE-1) == 0)
        {
            cur->key[MESHX_KEY_NAME_MAX_SIZE - 1] = '\0';
            memcpy(cur->data, blob, blob_size);
            cur->len = blob_size;
            goto restart_timer;  // skip new node alloc
        }
        cur = cur->next;
    }

    // Allocate new node
    meshx_err_t m_err = MESHX_MALLOC(&node, sizeof(meshx_nvs_write_list_t));
    if (m_err != MESHX_SUCCESS || !node) return MESHX_NO_MEM;

    strncpy(node->key, key, MESHX_KEY_NAME_MAX_SIZE);
    memcpy(node->data, blob, blob_size);
    node->len = blob_size;
    node->next = meshx_nvs_write_list_head;
    meshx_nvs_write_list_head = node;

restart_timer:
    if (arm_timer)
    {
#if MESHX_NVS_TIMER_PERIOD
        timer_err = meshx_os_timer_restart(meshx_nvs_inst.meshx_nvs_commit_tmr);
        if (timer_err)
        {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "meshx_os_timer_restart failed: (%d)", timer_err);
        }
#else
        meshx_nvs_commit();
#endif /* MESHX_NVS_TIMER_PERIOD */
    }

    return MESHX_SUCCESS;
}

/*
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
meshx_err_t meshx_nvs_element_ctx_get(uint16_t element_id, meshx_element_type_t element_type, void *blob, size_t blob_size)
{
    char key[MESHX_KEY_NAME_MAX_SIZE];
    meshx_tiny_snprintf(key, MESHX_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, (uint8_t)element_type, element_id);
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "meshx_nvs_element_ctx_get: key=%s, size=%d", key, (int)blob_size);
    return meshx_nvs_get(key, blob, blob_size);
}

/*
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
meshx_err_t meshx_nvs_element_ctx_set(uint16_t element_id, meshx_element_type_t element_type, const void *blob, size_t blob_size)
{
    char key[MESHX_KEY_NAME_MAX_SIZE];
    meshx_tiny_snprintf(key, MESHX_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, (uint8_t)element_type, element_id);
    return meshx_nvs_set(key, blob, (uint16_t) blob_size, MESHX_NVS_AUTO_COMMIT);
}

/*
 * @brief Remove the context of a specific element from NVS.
 *
 * @param[in] element_id   The ID of the element whose context is to be removed.
 * @param[in] element_type The type of the element.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully removed the context.
 */
meshx_err_t meshx_nvs_element_ctx_remove(uint16_t element_id, meshx_element_type_t element_type)
{
    char key[MESHX_KEY_NAME_MAX_SIZE];
    meshx_err_t err = MESHX_SUCCESS;
    meshx_err_t temp_err = MESHX_SUCCESS;
    uint8_t t = 0;

    if (element_type == MESHX_ELEMENT_TYPE_ALL)
    {
        for (t = 0; t < (uint8_t)MESHX_ELEMENT_TYPE_MAX; t++)
        {
            snprintf(key, MESHX_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, t, element_id);
            temp_err = meshx_nvs_remove(key);
            if (temp_err != MESHX_SUCCESS && temp_err != MESHX_NOT_FOUND)
            {
                err = temp_err;
            }
        }
        return err;
    }
    snprintf(key, MESHX_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, (uint8_t)element_type, element_id);
    meshx_tiny_snprintf(key, MESHX_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, (uint8_t)element_type, element_id);
    return meshx_nvs_remove(key);
}

/*
 * @brief Calculate CRC-16-CCITT checksum over a buffer.
 *
 * @param[in] data Pointer to data buffer.
 * @param[in] len  Length of data buffer in bytes.
 *
 * @return uint16_t Calculated 16-bit CRC checksum.
 */
static uint16_t meshx_nvs_calc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i = 0;
    int j = 0;

    for (i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/*
 * @brief Store composition schema into NVS.
 *
 * Validates the schema, computes the CRC-16-CCITT, and writes the
 * record synchronously to NVS.
 *
 * @param[in] schema Pointer to composition schema structure.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully stored the schema.
 *     - MESHX_INVALID_ARG: Null pointer or invalid element count.
 *     - Error code from meshx_nvs_set on write failure.
 */
meshx_err_t meshx_nvs_comp_schema_set(const meshx_nvs_comp_schema_t *schema)
{
    meshx_nvs_comp_schema_t to_write;
    size_t crc_len = 0;

    if (!schema || schema->element_count > CONFIG_MAX_ELEMENT_COUNT) {
        return MESHX_INVALID_ARG;
    }

    to_write = *schema;
    to_write.magic = MESHX_NVS_COMP_MAGIC;
    to_write.version = MESHX_NVS_COMP_VERSION;
    to_write.reserved = 0;

    crc_len = offsetof(meshx_nvs_comp_schema_t, schema_crc);
    to_write.schema_crc = meshx_nvs_calc_crc16((const uint8_t *)&to_write, crc_len);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Saving NVS comp schema (%d elements, CRC: 0x%04X, flags: 0x%02X)",
               to_write.element_count, to_write.schema_crc, to_write.flags);

    return meshx_nvs_set(MESHX_NVS_KEY_COMP_SCHEMA, &to_write, sizeof(to_write), MESHX_NVS_AUTO_COMMIT);
}

/*
 * @brief Retrieve composition schema from NVS with CRC validation.
 *
 * Reads the schema from NVS, verifies magic number, version, element count,
 * and CRC-16-CCITT checksum before returning.
 *
 * @param[out] schema Pointer to buffer to receive the schema.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully retrieved and validated the schema.
 *     - MESHX_INVALID_ARG: Null pointer passed.
 *     - MESHX_INVALID_STATE: Magic, version, CRC, or element count corrupted.
 *     - Error code from meshx_nvs_get if read fails.
 */
meshx_err_t meshx_nvs_comp_schema_get(meshx_nvs_comp_schema_t *schema)
{
    meshx_nvs_comp_schema_t read_buf;
    meshx_err_t err = MESHX_SUCCESS;
    size_t crc_len = 0;
    uint16_t expected_crc = 0;

    if (!schema) {
        return MESHX_INVALID_ARG;
    }

    err = meshx_nvs_get(MESHX_NVS_KEY_COMP_SCHEMA, &read_buf, sizeof(read_buf));
    if (err != MESHX_SUCCESS) {
        return err;
    }

    if (read_buf.magic != MESHX_NVS_COMP_MAGIC || read_buf.version != MESHX_NVS_COMP_VERSION) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_NVS, "Invalid NVS comp schema magic (0x%08X) or version (%d)",
                   (unsigned int)read_buf.magic, (int)read_buf.version);
        return MESHX_INVALID_STATE;
    }

    crc_len = offsetof(meshx_nvs_comp_schema_t, schema_crc);
    expected_crc = meshx_nvs_calc_crc16((const uint8_t *)&read_buf, crc_len);
    if (expected_crc != read_buf.schema_crc) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "NVS comp schema CRC mismatch (calc: 0x%04X, stored: 0x%04X)",
                   expected_crc, read_buf.schema_crc);
        return MESHX_INVALID_STATE;
    }

    if (read_buf.element_count > CONFIG_MAX_ELEMENT_COUNT) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "NVS comp schema element count exceeded: %d", read_buf.element_count);
        return MESHX_INVALID_STATE;
    }

    *schema = read_buf;
    return MESHX_SUCCESS;
}

/*
 * @brief Erase composition schema from NVS.
 *
 * Removes the composition schema record from the NVS namespace.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully erased.
 *     - Error code from meshx_nvs_remove on failure.
 */
meshx_err_t meshx_nvs_comp_schema_erase(void)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Erasing NVS composition schema");
    return meshx_nvs_remove(MESHX_NVS_KEY_COMP_SCHEMA);
}

/*
 * @brief Store link table into NVS with CRC validation.
 *
 * @param[in] table Pointer to link table structure.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_set(const meshx_nvs_link_table_t *table)
{
    meshx_nvs_link_table_t to_write;
    size_t crc_len = 0;

    if (!table || table->link_count > MESHX_MAX_PERSISTED_LINKS) {
        return MESHX_INVALID_ARG;
    }

    to_write = *table;
    to_write.magic = MESHX_NVS_LINK_MAGIC;
    to_write.version = MESHX_NVS_LINK_VERSION;
    to_write.reserved = 0;

    crc_len = offsetof(meshx_nvs_link_table_t, table_crc);
    to_write.table_crc = meshx_nvs_calc_crc16((const uint8_t *)&to_write, crc_len);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Saving NVS link table (%d active links, epoch: %u, CRC: 0x%04X)",
               to_write.link_count, to_write.link_epoch, to_write.table_crc);

    return meshx_nvs_set(MESHX_NVS_KEY_LINK_TABLE, &to_write, sizeof(to_write), MESHX_NVS_AUTO_COMMIT);
}

/*
 * @brief Retrieve link table from NVS with CRC validation.
 *
 * @param[out] table Pointer to buffer to receive link table.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_get(meshx_nvs_link_table_t *table)
{
    meshx_nvs_link_table_t read_buf;
    meshx_err_t err = MESHX_SUCCESS;
    size_t crc_len = 0;
    uint16_t expected_crc = 0;

    if (!table) {
        return MESHX_INVALID_ARG;
    }

    err = meshx_nvs_get(MESHX_NVS_KEY_LINK_TABLE, &read_buf, sizeof(read_buf));
    if (err != MESHX_SUCCESS) {
        return err;
    }

    if (read_buf.magic != MESHX_NVS_LINK_MAGIC || read_buf.version != MESHX_NVS_LINK_VERSION) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_NVS, "Invalid NVS link table magic (0x%08X) or version (%d)",
                   (unsigned int)read_buf.magic, (int)read_buf.version);
        return MESHX_INVALID_STATE;
    }

    crc_len = offsetof(meshx_nvs_link_table_t, table_crc);
    expected_crc = meshx_nvs_calc_crc16((const uint8_t *)&read_buf, crc_len);
    if (expected_crc != read_buf.table_crc) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "NVS link table CRC mismatch (calc: 0x%04X, stored: 0x%04X)",
                   expected_crc, read_buf.table_crc);
        return MESHX_INVALID_STATE;
    }

    if (read_buf.link_count > MESHX_MAX_PERSISTED_LINKS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "NVS link table link count exceeded: %d", read_buf.link_count);
        return MESHX_INVALID_STATE;
    }

    *table = read_buf;
    return MESHX_SUCCESS;
}

/*
 * @brief Erase link table from NVS.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nvs_link_table_erase(void)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "Erasing NVS link table");
    return meshx_nvs_remove(MESHX_NVS_KEY_LINK_TABLE);
}

/*
 * @brief Get the current link epoch counter from NVS.
 *
 * @return Current link epoch (defaults to 0 if not yet set or unlinked).
 */
uint16_t meshx_nvs_get_link_epoch(void)
{
    meshx_nvs_link_table_t table;
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS) {
        return table.link_epoch;
    }
    return 0;
}

/**
 * @brief Get the persistent Key Rotation epoch counter from NVS.
 */
uint32_t meshx_nvs_get_kr_epoch(void)
{
    uint32_t epoch = 0;
    if (meshx_nvs_get(MESHX_NVS_KEY_KR_EPOCH, &epoch, sizeof(epoch)) == MESHX_SUCCESS) {
        return epoch;
    }
    return 0;
}

/**
 * @brief Commit the Key Rotation epoch counter to NVS.
 * @param[in]   epoch   Epoch no to be written
 *
 * @return      MESHX_SUCCESS on success, or error code
 */
meshx_err_t meshx_nvs_set_kr_epoch(uint32_t epoch)
{
    meshx_err_t err = meshx_nvs_set(MESHX_NVS_KEY_KR_EPOCH, &epoch, sizeof(epoch), MESHX_NVS_AUTO_COMMIT);
    if (err == MESHX_SUCCESS) {
        meshx_nvs_commit();
    }
    return err;
}

#if CONFIG_ENABLE_UNIT_TEST

/*
 * @brief MeshX NVS unit test command IDs.
 */
typedef enum meshx_nvs_cli_cmd{
    MESHX_NVS_CLI_CMD_OPEN,         /* ut 3 0 0 */
    MESHX_NVS_CLI_CMD_SET,          /* ut 3 1 1 [arm_timer?] */
    MESHX_NVS_CLI_CMD_GET,          /* ut 3 2 0 */
    MESHX_NVS_CLI_CMD_COMMIT,       /* ut 3 3 0 */
    MESHX_NVS_CLI_CMD_REMOVE,       /* ut 3 4 0 */
    MESHX_NVS_CLI_CMD_ERASE,        /* ut 3 5 0 */
    MESHX_NVS_CLI_CMD_CLOSE,        /* ut 3 6 0 */
    MESHX_NVS_CLI_CMD_SCHEMA_SET,   /* ut 3 7 1 [count] */
    MESHX_NVS_CLI_CMD_SCHEMA_GET,   /* ut 3 8 0 */
    MESHX_NVS_CLI_CMD_SCHEMA_ERASE, /* ut 3 9 0 */
    MESHX_NVS_CLI_CMD_SCHEMA_CORRUPT,/* ut 3 10 0 */
    MESHX_NVS_CLI_CMD_LINK_SET,     /* ut 3 11 0 */
    MESHX_NVS_CLI_CMD_LINK_GET,     /* ut 3 12 0 */
    MESHX_NVS_CLI_CMD_LINK_ERASE,   /* ut 3 13 0 */
    MESHX_NVS_CLI_CMD_LINK_RECONCILE,/* ut 3 14 0 */
    MESHX_NVS_CLI_CMD_GROUP_ALLOC_TEST,/* ut 3 15 0 */
    MESHX_NVS_CLI_MAX
}meshx_nvs_cli_cmd_t;

/*
 * @brief Callback handler for MeshX NVS unit test command.
 *
 * This function handles MeshX NVS unit test command by processing the
 * provided command ID and arguments.
 *
 * @param[in] cmd_id    The command ID to be processed.
 * @param[in] argc      The number of arguments provided.
 * @param[in] argv      The array of arguments.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static meshx_err_t meshx_nvs_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    meshx_err_t err = MESHX_SUCCESS;
    uint32_t ut_blob = 0xDEAD;
    uint32_t ut_blob_get = 0x00;
    bool arm_timer = false;
    meshx_nvs_comp_schema_t test_schema;
    meshx_nvs_comp_schema_t read_schema;
    meshx_nvs_link_table_t  test_link_table;
    meshx_nvs_link_table_t  read_link_table;
    uint8_t count = 0;
    uint8_t i = 0;

    MESHX_UNUSED(argc);
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (cmd_id >= MESHX_NVS_CLI_MAX)
    {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "Invalid number of arguments");
        return MESHX_INVALID_ARG;
    }

    switch(cmd_id)
    {
        case MESHX_NVS_CLI_CMD_OPEN:
            /* MeshX NVS Open uses defaut values */
            err = meshx_nvs_open(CONFIG_CID_ID, CONFIG_PID_ID, 0);
            break;
        case MESHX_NVS_CLI_CMD_SET:
            arm_timer = UT_GET_ARG(0, bool, argv) == 0 ? MESHX_NVS_NO_AUTO_COMMIT : MESHX_NVS_AUTO_COMMIT;
            err = meshx_nvs_set(MESHX_NVS_UNIT_TEST_KEY, &ut_blob, sizeof(ut_blob), arm_timer);
            break;
        case MESHX_NVS_CLI_CMD_GET:
            err = meshx_nvs_get(MESHX_NVS_UNIT_TEST_KEY, &ut_blob_get, sizeof(ut_blob_get));
            if(err == MESHX_SUCCESS && ut_blob != ut_blob_get)
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "MESHX NVS Integrety Test Failed");
            break;
        case MESHX_NVS_CLI_CMD_COMMIT:
            err = meshx_nvs_commit();
            break;
        case MESHX_NVS_CLI_CMD_REMOVE:
            err = meshx_nvs_remove(MESHX_NVS_UNIT_TEST_KEY);
            break;
        case MESHX_NVS_CLI_CMD_ERASE:
            err = meshx_nvs_erase();
            break;
        case MESHX_NVS_CLI_CMD_CLOSE:
            err = meshx_nvs_close();
            break;
        case MESHX_NVS_CLI_CMD_SCHEMA_SET:
            count = (argc > 0) ? (uint8_t)UT_GET_ARG(0, int, argv) : 3;
            if (count > CONFIG_MAX_ELEMENT_COUNT) {
                count = CONFIG_MAX_ELEMENT_COUNT;
            }
            memset(&test_schema, 0, sizeof(test_schema));
            test_schema.element_count = count;
            test_schema.flags = MESHX_COMP_FLAG_OVERRIDE_RO;
            for (i = 0; i < count; i++) {
                test_schema.element_types[i] = (uint16_t)(i % MESHX_ELEMENT_TYPE_MAX);
            }
            err = meshx_nvs_comp_schema_set(&test_schema);
            if (err == MESHX_SUCCESS) {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_SET PASS: %d elements, flags=0x%02X",
                           test_schema.element_count, test_schema.flags);
            } else {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_SET FAIL: err=%d", err);
            }
            break;
        case MESHX_NVS_CLI_CMD_SCHEMA_GET:
            memset(&read_schema, 0, sizeof(read_schema));
            err = meshx_nvs_comp_schema_get(&read_schema);
            if (err == MESHX_SUCCESS) {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS,
                           "UT SCHEMA_GET PASS: magic=0x%08X ver=%d count=%d flags=0x%02X crc=0x%04X",
                           (unsigned int)read_schema.magic, (int)read_schema.version,
                           (int)read_schema.element_count, (int)read_schema.flags,
                           (unsigned int)read_schema.schema_crc);
                for (i = 0; i < read_schema.element_count; i++) {
                    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "  Element[%d]: type=%d", (int)i, (int)read_schema.element_types[i]);
                }
            } else {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_GET FAIL: err=%d", err);
            }
            break;
        case MESHX_NVS_CLI_CMD_SCHEMA_ERASE:
            err = meshx_nvs_comp_schema_erase();
            if (err != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_ERASE FAIL: err=%d", err);
                break;
            }
            memset(&read_schema, 0, sizeof(read_schema));
            err = meshx_nvs_comp_schema_get(&read_schema);
            if (err != MESHX_SUCCESS) {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_ERASE PASS: schema successfully erased");
                err = MESHX_SUCCESS;
            } else {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_ERASE FAIL: schema still readable after erase");
                err = MESHX_INVALID_STATE;
            }
            break;
        case MESHX_NVS_CLI_CMD_SCHEMA_CORRUPT:
            memset(&test_schema, 0, sizeof(test_schema));
            test_schema.magic = MESHX_NVS_COMP_MAGIC;
            test_schema.version = MESHX_NVS_COMP_VERSION;
            test_schema.element_count = 2;
            test_schema.flags = MESHX_COMP_FLAG_OVERRIDE_RO;
            test_schema.element_types[0] = 0;
            test_schema.element_types[1] = 1;
            test_schema.schema_crc = 0xBADC; /* Intentionally corrupt CRC */
            err = meshx_nvs_set(MESHX_NVS_KEY_COMP_SCHEMA, &test_schema, sizeof(test_schema), MESHX_NVS_AUTO_COMMIT);
            if (err != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_CORRUPT write fail: err=%d", err);
                break;
            }
            memset(&read_schema, 0, sizeof(read_schema));
            err = meshx_nvs_comp_schema_get(&read_schema);
            if (err != MESHX_SUCCESS) {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_CORRUPT PASS: Corrupted CRC successfully rejected (err=%d)", err);
                err = MESHX_SUCCESS;
            } else {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT SCHEMA_CORRUPT FAIL: corrupted schema was accepted!");
                err = MESHX_INVALID_STATE;
            }
            break;
        case MESHX_NVS_CLI_CMD_LINK_SET:
            memset(&test_link_table, 0, sizeof(test_link_table));
            test_link_table.link_epoch = 1;
            test_link_table.link_count = 2;
            /* Link 0: Self-pairing (Intra-node 1:1 direct link: Element 0 Switch -> Element 1 Relay) */
            test_link_table.links[0].local_element_idx   = 0;
            test_link_table.links[0].bound_variant       = 1;
            test_link_table.links[0].target_unicast_addr = 0x0011;
            test_link_table.links[0].target_group_addr   = 0x0000;
            test_link_table.links[0].func_id             = 0x0000;
            test_link_table.links[0].active              = 1;
            /* Link 1: 1:N Group Link (Anchor = Switch 0x0010, group = 0xC010) */
            test_link_table.links[1].local_element_idx   = 0;
            test_link_table.links[1].bound_variant       = 1;
            test_link_table.links[1].target_unicast_addr = 0x0025;
            test_link_table.links[1].target_group_addr   = 0xC010;
            test_link_table.links[1].func_id             = 0x0000;
            test_link_table.links[1].active              = 1;

            err = meshx_nvs_link_table_set(&test_link_table);
            if (err == MESHX_SUCCESS)
            {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_SET PASS: epoch=%u count=%u",
                           test_link_table.link_epoch, test_link_table.link_count);
            }
            else
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_SET FAIL: err=%d", err);
            }
            break;

        case MESHX_NVS_CLI_CMD_LINK_GET:
            memset(&read_link_table, 0, sizeof(read_link_table));
            err = meshx_nvs_link_table_get(&read_link_table);
            if (err == MESHX_SUCCESS)
            {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS,
                           "UT LINK_GET PASS: magic=0x%08X ver=%d epoch=%u count=%d crc=0x%04X",
                           (unsigned int)read_link_table.magic, (int)read_link_table.version,
                           (unsigned int)read_link_table.link_epoch, (int)read_link_table.link_count,
                           (unsigned int)read_link_table.table_crc);
            }
            else
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_GET FAIL: err=%d", err);
            }
            break;

        case MESHX_NVS_CLI_CMD_LINK_ERASE:
            err = meshx_nvs_link_table_erase();
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_ERASE FAIL: err=%d", err);
                break;
            }
            memset(&read_link_table, 0, sizeof(read_link_table));
            err = meshx_nvs_link_table_get(&read_link_table);
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_ERASE PASS: link table successfully erased");
                err = MESHX_SUCCESS;
            }
            else
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_ERASE FAIL: link table still readable after erase");
                err = MESHX_INVALID_STATE;
            }
            break;

        case MESHX_NVS_CLI_CMD_LINK_RECONCILE:
            err = meshx_link_reconcile();
            if (err == MESHX_SUCCESS)
            {
                MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_RECONCILE PASS: new_epoch=%u",
                           meshx_nvs_get_link_epoch());
            }
            else
            {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT LINK_RECONCILE FAIL: err=%d", err);
            }
            break;

        case MESHX_NVS_CLI_CMD_GROUP_ALLOC_TEST:
            {
                bool all_passed = true;
                /* 1. Test mathematical Anchor Group Derivation */
                if (meshx_group_derive_from_anchor(0x0014) != 0xC014 ||
                    meshx_group_derive_from_anchor(0x03FF) != 0xC3FF ||
                    meshx_group_derive_from_anchor(0x0400) != 0xC000 ||
                    meshx_group_derive_from_anchor(0x0414) != 0xC014 ||
                    !meshx_is_dynamic_group_addr(0xC000) ||
                    !meshx_is_dynamic_group_addr(0xC3FF) ||
                    meshx_is_dynamic_group_addr(0xBFFF) ||
                    meshx_is_dynamic_group_addr(0xC400))
                {
                    MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT GROUP_ALLOC FAIL: Anchor math check failed");
                    all_passed = false;
                }

                /* 2. Test Anchor Node Resolution (1:N Producer hub vs N:1 Consumer hub) */
                uint16_t anchor_1n = meshx_group_resolve_anchor_topology(0x0010, 0x0020, MESHX_ROLE_PRODUCER, MESHX_ROLE_CONSUMER, MESHX_TOPOLOGY_1_TO_N);
                if (anchor_1n != 0x0010)
                {
                    MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT GROUP_ALLOC FAIL: 1:N anchor expected 0x0010, got 0x%04X", anchor_1n);
                    all_passed = false;
                }

                uint16_t anchor_n1 = meshx_group_resolve_anchor_topology(0x0010, 0x0020, MESHX_ROLE_PRODUCER, MESHX_ROLE_CONSUMER, MESHX_TOPOLOGY_N_TO_1);
                if (anchor_n1 != 0x0020)
                {
                    MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT GROUP_ALLOC FAIL: N:1 anchor expected 0x0020, got 0x%04X", anchor_n1);
                    all_passed = false;
                }

                /* 3. Test Reference Counting against NVS Link Table */
                memset(&test_link_table, 0, sizeof(test_link_table));
                test_link_table.link_epoch = 1;
                test_link_table.link_count = 3;
                test_link_table.links[0].target_group_addr = 0xC010;
                test_link_table.links[0].active = 1;
                test_link_table.links[1].target_group_addr = 0xC010;
                test_link_table.links[1].active = 1;
                test_link_table.links[2].target_group_addr = 0x0000;
                test_link_table.links[2].active = 1;
                meshx_nvs_link_table_set(&test_link_table);

                if (meshx_group_get_ref_count(0xC010) != 2 ||
                    meshx_group_get_ref_count(0x0000) != 0 ||
                    meshx_group_get_ref_count(0xC020) != 0)
                {
                    MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "UT GROUP_ALLOC FAIL: Ref count check failed");
                    all_passed = false;
                }

                if (all_passed)
                {
                    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_NVS, "UT GROUP_ALLOC PASS: all anchor and group allocation invariants verified");
                    err = MESHX_SUCCESS;
                }
                else
                {
                    err = MESHX_FAIL;
                }
            }
            break;
        default:
            err = MESHX_INVALID_ARG;
            break;
    }

    return err;
}
#endif /* CONFIG_ENABLE_UNIT_TEST */
