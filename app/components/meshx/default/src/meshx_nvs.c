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

#define MESHX_NVS_INIT_MAGIC        0x5489

#ifdef CONFIG_BLE_MESH_SPECIFIC_PARTITION
#define MESHX_NVS_PARTITION         CONFIG_BLE_MESH_PARTITION_NAME
#endif

#define MESHX_NVS_NAMESPACE         "MESHX_NVS"
#define MESHX_NVS_NAMESPACE_PID     "MESHX_PID"
#define MESHX_NVS_NAMESPACE_CID     "MESHX_CID"
#define MESHX_NVS_TIMER_NAME        "MESHX_COMMIT_TIMER"
#define MESHX_NVS_ELEMENT_CTX       "MESHX_EL_%04x"
#define MESHX_NVS_RELOAD_ONE_SHOT   pdFALSE

#if CONFIG_ENABLE_UNIT_TEST
#define MESHX_NVS_UNIT_TEST_KEY     "MESHX_UT"

static meshx_err_t meshx_nvs_unit_test_cb_handler(int cmd_id, int argc, char **argv);
#endif

/**
 * @brief: MeshX NVS Instance
 */
static meshx_nvs_t meshx_nvs_inst;

#if MESHX_NVS_TIMER_PERIOD
/**
 * @brief MeshX NVS Timer callback.
 * @note This is responsible for performing the commit after stability timeout
 *
 * @param[in]   p_timer     os_timer param pointer
 * @return None
 *
 */
static void meshx_nvs_os_timer_cb(const os_timer_t *p_timer)
{
    meshx_err_t err = MESHX_SUCCESS;
    ESP_LOGD(TAG, "%s fire", OS_TMER_GET_TIMER_NAME(p_timer));

    err = meshx_nvs_commit();
    if (err)
        ESP_LOGE(TAG, "meshx_nvs_commit %p", (void *)err);
}
#endif /* MESHX_NVS_TIMER_PERIOD */

/**
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
        ESP_LOGE(TAG, "meshx_nvs_erase %p", (void *)err);
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
        ESP_LOGE(TAG, "meshx_nvs_set %p", (void *)err);
        return err;
    }

    err = meshx_nvs_set(
        MESHX_NVS_NAMESPACE_CID,
        &(meshx_nvs_inst.cid),
        sizeof(meshx_nvs_inst.cid),
        MESHX_NVS_AUTO_COMMIT);

    if (err)
    {
        ESP_LOGE(TAG, "meshx_nvs_set %p", (void *)err);
        return err;
    }

    return err;
}

/**
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
        ESP_LOGE(TAG, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */
    return err;
}

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
meshx_err_t meshx_nvs_open(uint16_t cid, uint16_t pid, uint32_t commit_timeout_ms)
{
    if (meshx_nvs_inst.init == MESHX_NVS_INIT_MAGIC)
    {
        return MESHX_INVALID_STATE;
    }

    meshx_err_t err;

#ifndef CONFIG_BLE_MESH_SPECIFIC_PARTITION
    err = nvs_open(
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        &(meshx_nvs_inst.meshx_nvs_handle));
#else
    err = nvs_open_from_partition(
        MESHX_NVS_PARTITION,
        MESHX_NVS_NAMESPACE,
        NVS_READWRITE,
        &(meshx_nvs_inst.meshx_nvs_handle));
#endif /* CONFIG_BLE_MESH_SPECIFIC_PARTITION */
    if (err)
    {
        ESP_LOGE(TAG, "nvs_open %p", (void *)err);
        return err;
    }

#if MESHX_NVS_TIMER_PERIOD
    if(commit_timeout_ms == 0)
    {
        commit_timeout_ms = MESHX_NVS_TIMER_PERIOD;
    }
    err = os_timer_create(
        MESHX_NVS_TIMER_NAME,
        commit_timeout_ms,
        MESHX_NVS_RELOAD_ONE_SHOT,
        &meshx_nvs_os_timer_cb,
        &(meshx_nvs_inst.meshx_nvs_stability_timer));
    if (err)
    {
        ESP_LOGE(TAG, "os_timer_create %p", (void *)err);
        return err;
    }
#else
    ESP_UNUSED(commit_timeout_ms);
#endif /* MESHX_NVS_TIMER_PERIOD */

    meshx_nvs_inst.init = MESHX_NVS_INIT_MAGIC;

    err = meshx_nvs_get(
        MESHX_NVS_NAMESPACE_CID,
            &(meshx_nvs_inst.cid),
        sizeof(meshx_nvs_inst.cid));

    err += meshx_nvs_get(
        MESHX_NVS_NAMESPACE_PID,
            &(meshx_nvs_inst.pid),
        sizeof(meshx_nvs_inst.pid));

    if(err != MESHX_SUCCESS)
    {
        ESP_LOGW(TAG, "Product ID not found in NVS reinitializing MeshX NVS");
        err = meshx_nvs_erase_prod_init(cid, pid);
    }

    else
    {
        if (meshx_nvs_inst.cid == cid && meshx_nvs_inst.pid == pid)
        {
            ESP_LOGI(TAG, "Product ID match: %x|%x", meshx_nvs_inst.pid, meshx_nvs_inst.cid);
        }
        else
        {
            ESP_LOGW(TAG, "Product ID mismatch: %x|%x", meshx_nvs_inst.pid, meshx_nvs_inst.cid);
            err = meshx_nvs_erase_prod_init(cid, pid);
        }
    }

    return err;
}

/**
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

    return nvs_erase_all(meshx_nvs_inst.meshx_nvs_handle);
}

/**
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

    return nvs_commit(meshx_nvs_inst.meshx_nvs_handle);
}

/**
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

    nvs_close(meshx_nvs_inst.meshx_nvs_handle);

#if MESHX_NVS_TIMER_PERIOD
    err = os_timer_delete(&(meshx_nvs_inst.meshx_nvs_stability_timer));
#endif /* MESHX_NVS_TIMER_PERIOD */
    meshx_nvs_inst.init = 0;
    return err;
}

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
meshx_err_t meshx_nvs_remove(char const *key)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    return nvs_erase_key(meshx_nvs_inst.meshx_nvs_handle, key);
}

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
meshx_err_t meshx_nvs_get(char const *key, void *blob, size_t blob_size)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;
    size_t b_size = blob_size;
    return nvs_get_blob(meshx_nvs_inst.meshx_nvs_handle, key, blob, &b_size);
}

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
meshx_err_t meshx_nvs_set(char const* key, void const* blob, size_t blob_size, bool arm_timer)
{
    if (meshx_nvs_inst.init != MESHX_NVS_INIT_MAGIC)
        return MESHX_INVALID_STATE;

    if(arm_timer)
    {
        /* Trigger the stability timer to commit the changes */
        meshx_err_t err = os_timer_restart(meshx_nvs_inst.meshx_nvs_stability_timer);
        if(err)
            ESP_LOGE(TAG, "os_timer_restart err:  %p", (void*) err);
    }

    return nvs_set_blob(meshx_nvs_inst.meshx_nvs_handle, key, blob, blob_size);
}

/**
 * @brief Retrieve the context of a specific element from NVS.
 *
 * This function fetches the stored context of a given element identified by its ID from
 * the Non-Volatile Storage (NVS).
 *
 * @param[in]   element_id  The ID of the element whose context is to be retrieved.
 * @param[out]  blob        Pointer to the buffer where the retrieved context will be stored.
 * @param[in]   blob_size   Size of the buffer provided to store the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully retrieved the context.
 */
meshx_err_t meshx_nvs_elemnt_ctx_get(uint16_t element_id, void *blob, size_t blob_size)
{
    char key[NVS_KEY_NAME_MAX_SIZE];
    snprintf(key, NVS_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, element_id);
    return meshx_nvs_get(key, blob, blob_size);
}

/**
 * @brief Store the context of a specific element to NVS.
 *
 * This function saves the context of a given element identified by its ID to
 * the Non-Volatile Storage (NVS).
 *
 * @param[in] element_id    The ID of the element whose context is to be stored.
 * @param[in] blob          Pointer to the buffer containing the context to be stored.
 * @param[in] blob_size     Size of the buffer containing the context.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully stored the context.
 */
meshx_err_t meshx_nvs_elemnt_ctx_set(uint16_t element_id, const void *blob, size_t blob_size)
{
    char key[NVS_KEY_NAME_MAX_SIZE];
    snprintf(key, NVS_KEY_NAME_MAX_SIZE, MESHX_NVS_ELEMENT_CTX, element_id);
    return meshx_nvs_set(key, blob, blob_size, MESHX_NVS_AUTO_COMMIT);
}

#if CONFIG_ENABLE_UNIT_TEST

/**
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
    MESHX_NVS_CLI_MAX
}meshx_nvs_cli_cmd_t;

/**
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
    bool arm_timer;

    ESP_LOGD(TAG, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (cmd_id >= MESHX_NVS_CLI_MAX)
    {
        ESP_LOGE(TAG, "Invalid number of arguments");
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
                ESP_LOGE(TAG, "MESHX NVS Integrety Test Failed");
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
        default:
            err = MESHX_INVALID_ARG;
            break;
    }

    return err;
}
#endif /* CONFIG_ENABLE_UNIT_TEST */
