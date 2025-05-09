/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_prov_srv.h
 * @brief This header file defines the provisioning server interface for the MeshX BLE Mesh stack.
 *        It includes data structures, enumerations, and function declarations for managing BLE Mesh
 *        provisioning operations, including node and provisioner functionalities.
 *
 *        The file provides definitions for provisioning events, callback parameters, and provisioning
 *        bearer and OOB information types. It also includes APIs for initializing and retrieving
 *        provisioning parameters.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_BLE_MESH_PROV_SRV_H__
#define __MESHX_BLE_MESH_PROV_SRV_H__

#include "meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

typedef control_task_msg_handle_t prov_srv_cb_t;
typedef control_task_msg_evt_provision_t prov_evt_t;

/*!< This enum value is associated with bt_mesh_prov_bearer_t in mesh_main.h */
typedef enum
{
    MESHX_PROV_ADV  = MESHX_BIT(0),
    MESHX_PROV_GATT = MESHX_BIT(1),
} meshx_prov_bearer_t;

/*!< This enum value is associated with bt_mesh_prov_oob_info_t in mesh_main.h */
typedef enum
{
    MESHX_PROV_OOB_OTHER        = MESHX_BIT(0),
    MESHX_PROV_OOB_URI          = MESHX_BIT(1),
    MESHX_PROV_OOB_2D_CODE      = MESHX_BIT(2),
    MESHX_PROV_OOB_BAR_CODE     = MESHX_BIT(3),
    MESHX_PROV_OOB_NFC          = MESHX_BIT(4),
    MESHX_PROV_OOB_NUMBER       = MESHX_BIT(5),
    MESHX_PROV_OOB_STRING       = MESHX_BIT(6),
    MESHX_PROV_CERT_BASED       = MESHX_BIT(7),
    MESHX_PROV_RECORDS          = MESHX_BIT(8),
    /* 9 - 10 are reserved */
    MESHX_PROV_OOB_ON_BOX       = MESHX_BIT(11),
    MESHX_PROV_OOB_IN_BOX       = MESHX_BIT(12),
    MESHX_PROV_OOB_ON_PAPER     = MESHX_BIT(13),
    MESHX_PROV_OOB_IN_MANUAL    = MESHX_BIT(14),
    MESHX_PROV_OOB_ON_DEV       = MESHX_BIT(15),
} meshx_prov_oob_info_t;

/**
 * @brief BLE Mesh Node/Provisioner callback parameters union
 */
typedef union
{
    /**
     * @brief MESHX_PROV_REGISTER_COMP_EVT
     */
    struct meshx_prov_register_comp_param
    {
        int err_code;     /*!< Indicate the result of BLE Mesh initialization */
    } prov_register_comp; /*!< Event parameter of MESHX_PROV_REGISTER_COMP_EVT */
    /**
     * @brief MESHX_NODE_SET_UNPROV_DEV_NAME_COMP_EVT
     */
    struct meshx_set_unprov_dev_name_comp_param
    {
        int err_code;                /*!< Indicate the result of setting BLE Mesh device name */
    } node_set_unprov_dev_name_comp; /*!< Event parameter of MESHX_NODE_SET_UNPROV_DEV_NAME_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROV_ENABLE_COMP_EVT
     */
    struct meshx_prov_enable_comp_param
    {
        int err_code;        /*!< Indicate the result of enabling BLE Mesh device */
    } node_prov_enable_comp; /*!< Event parameter of MESHX_NODE_PROV_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROV_DISABLE_COMP_EVT
     */
    struct meshx_prov_disable_comp_param
    {
        int err_code;         /*!< Indicate the result of disabling BLE Mesh device */
    } node_prov_disable_comp; /*!< Event parameter of MESHX_NODE_PROV_DISABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROV_LINK_OPEN_EVT
     */
    struct meshx_link_open_evt_param
    {
        meshx_prov_bearer_t bearer; /*!< Type of the bearer used when device link is open */
    } node_prov_link_open;          /*!< Event parameter of MESHX_NODE_PROV_LINK_OPEN_EVT */
    /**
     * @brief MESHX_NODE_PROV_LINK_CLOSE_EVT
     */
    struct meshx_link_close_evt_param
    {
        meshx_prov_bearer_t bearer; /*!< Type of the bearer used when device link is closed */
        uint8_t reason;             /*!< Reason of the closed provisioning link */
    } node_prov_link_close;         /*!< Event parameter of MESHX_NODE_PROV_LINK_CLOSE_EVT */
    /**
     * @brief MESHX_NODE_PROV_OUTPUT_NUMBER_EVT
     */
    struct meshx_output_num_evt_param
    {
        meshx_output_action_t action; /*!< Action of Output OOB Authentication */
        uint32_t number;              /*!< Number of Output OOB Authentication  */
    } node_prov_output_num;           /*!< Event parameter of MESHX_NODE_PROV_OUTPUT_NUMBER_EVT */
    /**
     * @brief MESHX_NODE_PROV_OUTPUT_STRING_EVT
     */
    struct meshx_output_str_evt_param
    {
        char string[8];     /*!< String of Output OOB Authentication */
    } node_prov_output_str; /*!< Event parameter of MESHX_NODE_PROV_OUTPUT_STRING_EVT */
    /**
     * @brief MESHX_NODE_PROV_INPUT_EVT
     */
    struct meshx_input_evt_param
    {
        meshx_input_action_t action; /*!< Action of Input OOB Authentication */
        uint8_t size;                /*!< Size of Input OOB Authentication */
    } node_prov_input;               /*!< Event parameter of MESHX_NODE_PROV_INPUT_EVT */
    /**
     * @brief MESHX_NODE_PROV_COMPLETE_EVT
     */
    struct meshx_provision_complete_evt_param
    {
        uint16_t net_idx;    /*!< NetKey Index */
        uint8_t net_key[16]; /*!< NetKey */
        uint16_t addr;       /*!< Primary address */
        uint8_t flags;       /*!< Flags */
        uint32_t iv_index;   /*!< IV Index */
    } node_prov_complete;    /*!< Event parameter of MESHX_NODE_PROV_COMPLETE_EVT */
    /**
     * @brief MESHX_NODE_PROV_RESET_EVT
     */
    struct meshx_provision_reset_param
    {

    } node_prov_reset; /*!< Event parameter of MESHX_NODE_PROV_RESET_EVT */
    /**
     * @brief MESHX_NODE_PROV_SET_OOB_PUB_KEY_COMP_EVT
     */
    struct meshx_set_oob_pub_key_comp_param
    {
        int err_code;                 /*!< Indicate the result of setting OOB Public Key */
    } node_prov_set_oob_pub_key_comp; /*!< Event parameter of MESHX_NODE_PROV_SET_OOB_PUB_KEY_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROV_INPUT_NUM_COMP_EVT
     */
    struct meshx_input_number_comp_param
    {
        int err_code;           /*!< Indicate the result of inputting number */
    } node_prov_input_num_comp; /*!< Event parameter of MESHX_NODE_PROV_INPUT_NUM_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROV_INPUT_STR_COMP_EVT
     */
    struct meshx_input_string_comp_param
    {
        int err_code;           /*!< Indicate the result of inputting string */
    } node_prov_input_str_comp; /*!< Event parameter of MESHX_NODE_PROV_INPUT_STR_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROXY_IDENTITY_ENABLE_COMP_EVT
     */
    struct meshx_proxy_identity_enable_comp_param
    {
        int err_code;                  /*!< Indicate the result of enabling Mesh Proxy advertising */
    } node_proxy_identity_enable_comp; /*!< Event parameter of MESHX_NODE_PROXY_IDENTITY_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROXY_GATT_ENABLE_COMP_EVT
     */
    struct meshx_proxy_gatt_enable_comp_param
    {
        int err_code;              /*!< Indicate the result of enabling Mesh Proxy Service */
    } node_proxy_gatt_enable_comp; /*!< Event parameter of MESHX_NODE_PROXY_GATT_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PROXY_GATT_DISABLE_COMP_EVT
     */
    struct meshx_proxy_gatt_disable_comp_param
    {
        int err_code;               /*!< Indicate the result of disabling Mesh Proxy Service */
    } node_proxy_gatt_disable_comp; /*!< Event parameter of MESHX_NODE_PROXY_GATT_DISABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PRIVATE_PROXY_IDENTITY_ENABLE_COMP_EVT
     */
    struct meshx_proxy_private_identity_enable_comp_param
    {
        int err_code;                          /*!< Indicate the result of enabling Mesh Proxy private advertising */
    } node_private_proxy_identity_enable_comp; /*!< Event parameter of MESHX_NODE_PRIVATE_PROXY_IDENTITY_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_PRIVATE_PROXY_IDENTITY_DISABLE_COMP_EVT
     */
    struct meshx_proxy_private_identity_disable_comp_param
    {
        int err_code;                           /*!< Indicate the result of disabling Mesh Proxy private advertising */
    } node_private_proxy_identity_disable_comp; /*!< Event parameter of MESHX_NODE_PRIVATE_PROXY_IDENTITY_DISABLE_COMP_EVT */
    /**
     * @brief MESHX_NODE_ADD_LOCAL_NET_KEY_COMP_EVT
     */
    struct meshx_node_add_local_net_key_comp_param
    {
        int err_code;        /*!< Indicate the result of adding local NetKey by the node */
        uint16_t net_idx;    /*!< NetKey Index */
    } node_add_net_key_comp; /*!< Event parameter of MESHX_NODE_ADD_LOCAL_NET_KEY_COMP_EVT */
    /**
     * @brief MESHX_NODE_ADD_LOCAL_APP_KEY_COMP_EVT
     */
    struct meshx_node_add_local_app_key_comp_param
    {
        int err_code;        /*!< Indicate the result of adding local AppKey by the node */
        uint16_t net_idx;    /*!< NetKey Index */
        uint16_t app_idx;    /*!< AppKey Index */
    } node_add_app_key_comp; /*!< Event parameter of MESHX_NODE_ADD_LOCAL_APP_KEY_COMP_EVT */
    /**
     * @brief MESHX_NODE_BIND_APP_KEY_TO_MODEL_COMP_EVT
     */
    struct meshx_node_bind_local_mod_app_comp_param
    {
        int err_code;                  /*!< Indicate the result of binding AppKey with model by the node */
        uint16_t element_addr;         /*!< Element address */
        uint16_t app_idx;              /*!< AppKey Index */
        uint16_t company_id;           /*!< Company ID */
        uint16_t model_id;             /*!< Model ID */
    } node_bind_app_key_to_model_comp; /*!< Event parameter of MESHX_NODE_BIND_APP_KEY_TO_MODEL_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT
     */
    struct meshx_provisioner_recv_unprov_adv_pkt_param
    {
        uint8_t dev_uuid[16];          /*!< Device UUID of the unprovisioned device */
        meshx_bd_addr_t addr;          /*!< Device address of the unprovisioned device */
        meshx_addr_type_t addr_type;   /*!< Device address type */
        uint16_t oob_info;             /*!< OOB Info of the unprovisioned device */
        uint8_t adv_type;              /*!< Advertising type of the unprovisioned device */
        meshx_prov_bearer_t bearer;    /*!< Bearer of the unprovisioned device */
        int8_t rssi;                   /*!< RSSI of the received advertising packet */
    } provisioner_recv_unprov_adv_pkt; /*!< Event parameter of MESHX_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_ENABLE_COMP_EVT
     */
    struct meshx_provisioner_prov_enable_comp_param
    {
        int err_code;               /*!< Indicate the result of enabling BLE Mesh Provisioner */
    } provisioner_prov_enable_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_DISABLE_COMP_EVT
     */
    struct meshx_provisioner_prov_disable_comp_param
    {
        int err_code;                /*!< Indicate the result of disabling BLE Mesh Provisioner */
    } provisioner_prov_disable_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_DISABLE_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_LINK_OPEN_EVT
     */
    struct meshx_provisioner_link_open_evt_param
    {
        meshx_prov_bearer_t bearer; /*!< Type of the bearer used when Provisioner link is opened */
    } provisioner_prov_link_open;   /*!< Event parameter of MESHX_PROVISIONER_PROV_LINK_OPEN_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_EVT
     */
    struct meshx_provisioner_prov_read_oob_pub_key_evt_param
    {
        uint8_t link_idx;                /*!< Index of the provisioning link */
    } provisioner_prov_read_oob_pub_key; /*!< Event parameter of MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_INPUT_EVT
     */
    struct meshx_provisioner_prov_input_evt_param
    {
        meshx_oob_method_t method;    /*!< Method of device Output OOB Authentication */
        meshx_output_action_t action; /*!< Action of device Output OOB Authentication */
        uint8_t size;                 /*!< Size of device Output OOB Authentication */
        uint8_t link_idx;             /*!< Index of the provisioning link */
    } provisioner_prov_input;         /*!< Event parameter of MESHX_PROVISIONER_PROV_INPUT_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_OUTPUT_EVT
     */
    struct meshx_provisioner_prov_output_evt_param
    {
        meshx_oob_method_t method;   /*!< Method of device Input OOB Authentication */
        meshx_input_action_t action; /*!< Action of device Input OOB Authentication */
        uint8_t size;                /*!< Size of device Input OOB Authentication */
        uint8_t link_idx;            /*!< Index of the provisioning link */
        /** Union of output OOB */
        union
        {
            char string[8];  /*!< String output by the Provisioner */
            uint32_t number; /*!< Number output by the Provisioner */
        };
    } provisioner_prov_output; /*!< Event parameter of MESHX_PROVISIONER_PROV_OUTPUT_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_LINK_CLOSE_EVT
     */
    struct meshx_provisioner_link_close_evt_param
    {
        meshx_prov_bearer_t bearer; /*!< Type of the bearer used when Provisioner link is closed */
        uint8_t reason;             /*!< Reason of the closed provisioning link */
    } provisioner_prov_link_close;  /*!< Event parameter of MESHX_PROVISIONER_PROV_LINK_CLOSE_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_COMPLETE_EVT
     */
    struct meshx_provisioner_prov_comp_param
    {
        uint16_t node_idx;       /*!< Index of the provisioned device */
        uint8_t device_uuid[16]; /*!< Device UUID of the provisioned device */
        uint16_t unicast_addr;   /*!< Primary address of the provisioned device */
        uint8_t element_num;     /*!< Element count of the provisioned device */
        uint16_t netkey_idx;     /*!< NetKey Index of the provisioned device */
    } provisioner_prov_complete; /*!< Event parameter of MESHX_PROVISIONER_PROV_COMPLETE_EVT */
    /**
     * @brief MESHX_PROVISIONER_CERT_BASED_PROV_START_EVT
     */
    struct meshx_provisioner_cert_based_prov_start_evt_param
    {
        uint16_t link_idx;               /*!< Index of the provisioning link */
    } provisioner_cert_based_prov_start; /*!< Event parameter of MESHX_PROVISIONER_CERT_BASED_PROV_START_EVT */
    /**
     * @brief MESHX_PROVISIONER_RECV_PROV_RECORDS_LIST_EVT
     */
    struct meshx_provisioner_recv_prov_records_list_evt_param
    {
        uint16_t link_idx;           /*!< Index of the provisioning link */
        uint16_t len;                /*!< Length of message */
        uint8_t *msg;                /*!< Lists the Record IDs of the provisioning records stored on the Provisionee */
    } recv_provisioner_records_list; /*!< Event parameter of MESHX_PROVISIONER_RECV_PROV_RECORDS_LIST_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_RECORD_RECV_COMP_EVT
     */
    struct meshx_provisioner_prov_record_recv_comp_evt_param
    {
        uint8_t status;                  /*!< Indicates whether or not the request was handled successfully */
        uint16_t link_idx;               /*!< Index of the provisioning link */
        uint16_t record_id;              /*!< Identifies the provisioning record for which the request is made */
        uint16_t frag_offset;            /*!< The starting offset of the requested fragment in the provisioning record data */
        uint16_t total_len;              /*!< Total length of the provisioning record data stored on the Provisionee */
        uint8_t *record;                 /*!< Provisioning record data fragment */
    } provisioner_prov_record_recv_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_RECORD_RECV_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SEND_PROV_RECORDS_GET_EVT
     */
    struct meshx_provisioner_send_prov_records_get_evt_param
    {
        int err_code;               /*!< Indicate the result of send Provisioning Records List Get message */
        uint16_t link_idx;          /*!< Index of the provisioning link */
    } provisioner_send_records_get; /*!< Event parameter of MESHX_PROVISIONER_SEND_PROV_RECORDS_GET_EVT */
    /**
     * @brief MESHX_PROVISIONER_SEND_PROV_RECORD_REQUEST_EVT
     */
    struct meshx_provisioner_send_prov_record_req_evt_param
    {
        int err_code;              /*!< Indicate the result of send Provisioning Record Request message */
        uint16_t link_idx;         /*!< Index of the provisioning link */
        uint16_t record_id;        /*!< Identifies the provisioning record for which the request is made */
        uint16_t frag_offset;      /*!< The starting offset of the requested fragment in the provisioning record data */
        uint16_t max_size;         /*!< The maximum size of the provisioning record fragment that the Provisioner can receive */
    } provisioner_send_record_req; /*!< Event parameter of MESHX_PROVISIONER_SEND_PROV_RECORD_REQUEST_EVT */
    /**
     * @brief MESHX_PROVISIONER_SEND_PROV_INVITE_EVT
     */
    struct meshx_provisioner_send_prov_invite_evt_param
    {
        uint16_t link_idx;          /*!< Index of the provisioning link */
        int err_code;               /*!< Indicate the result of send Provisioning Invite message */
    } provisioner_send_prov_invite; /*!< Event parameter of MESHX_PROVISIONER_SEND_PROV_INVITE_EVT */
    /**
     * @brief MESHX_PROVISIONER_SEND_LINK_CLOSE_EVT
     */
    struct meshx_provisioner_send_link_close_evt_param
    {
        uint16_t link_idx;         /*!< Index of the provisioning link */
        int err_code;              /*!< Indicate the result of send Link Close message */
    } provisioner_send_link_close; /*!< Event parameter of MESHX_PROVISIONER_SEND_LINK_CLOSE_EVT */
    /**
     * @brief MESHX_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT
     */
    struct meshx_provisioner_add_unprov_dev_comp_param
    {
        int err_code;                  /*!< Indicate the result of adding device into queue by the Provisioner */
    } provisioner_add_unprov_dev_comp; /*!< Event parameter of MESHX_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_DEV_WITH_ADDR_COMP_EVT
     */
    struct meshx_provisioner_prov_dev_with_addr_comp_param
    {
        int err_code;                      /*!< Indicate the result of Provisioner starting to provision a device */
    } provisioner_prov_dev_with_addr_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_DEV_WITH_ADDR_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_DELETE_DEV_COMP_EVT
     */
    struct meshx_provisioner_delete_dev_comp_param
    {
        int err_code;              /*!< Indicate the result of deleting device by the Provisioner */
    } provisioner_delete_dev_comp; /*!< Event parameter of MESHX_PROVISIONER_DELETE_DEV_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT
     */
    struct meshx_provisioner_set_dev_uuid_match_comp_param
    {
        int err_code;                      /*!< Indicate the result of setting Device UUID match value by the Provisioner */
    } provisioner_set_dev_uuid_match_comp; /*!< Event parameter of MESHX_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_PROV_DATA_INFO_COMP_EVT
     */
    struct meshx_provisioner_set_prov_data_info_comp_param
    {
        int err_code;                      /*!< Indicate the result of setting provisioning info by the Provisioner */
    } provisioner_set_prov_data_info_comp; /*!< Event parameter of MESHX_PROVISIONER_SET_PROV_DATA_INFO_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_STATIC_OOB_VALUE_COMP_EVT
     */
    struct meshx_provisioner_set_static_oob_val_comp_param
    {
        int err_code;                      /*!< Indicate the result of setting static oob value by the Provisioner */
    } provisioner_set_static_oob_val_comp; /*!< Event parameter of MESHX_PROVISIONER_SET_STATIC_OOB_VALUE_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_PRIMARY_ELEM_ADDR_COMP_EVT
     */
    struct meshx_provisioner_set_primary_elem_addr_comp_param
    {
        int err_code;                         /*!< Indicate the result of setting unicast address of primary element by the Provisioner */
    } provisioner_set_primary_elem_addr_comp; /*!< Event parameter of MESHX_PROVISIONER_SET_PRIMARY_ELEM_ADDR_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_COMP_EVT
     */
    struct meshx_provisioner_prov_read_oob_pub_key_comp_param
    {
        int err_code;                         /*!< Indicate the result of setting OOB Public Key by the Provisioner */
    } provisioner_prov_read_oob_pub_key_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_READ_OOB_PUB_KEY_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_INPUT_NUMBER_COMP_EVT
     */
    struct meshx_provisioner_prov_input_num_comp_param
    {
        int err_code;                  /*!< Indicate the result of inputting number by the Provisioner */
    } provisioner_prov_input_num_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_INPUT_NUMBER_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_PROV_INPUT_STRING_COMP_EVT
     */
    struct meshx_provisioner_prov_input_str_comp_param
    {
        int err_code;                  /*!< Indicate the result of inputting string by the Provisioner */
    } provisioner_prov_input_str_comp; /*!< Event parameter of MESHX_PROVISIONER_PROV_INPUT_STRING_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_NODE_NAME_COMP_EVT
     */
    struct meshx_provisioner_set_node_name_comp_param
    {
        int err_code;                 /*!< Indicate the result of setting provisioned device name by the Provisioner */
        uint16_t node_index;          /*!< Index of the provisioned device */
    } provisioner_set_node_name_comp; /*!< Event parameter of MESHX_PROVISIONER_SET_NODE_NAME_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT
     */
    struct meshx_provisioner_add_local_app_key_comp_param
    {
        int err_code;               /*!< Indicate the result of adding local AppKey by the Provisioner */
        uint16_t net_idx;           /*!< NetKey Index */
        uint16_t app_idx;           /*!< AppKey Index */
    } provisioner_add_app_key_comp; /*!< Event parameter of MESHX_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_UPDATE_LOCAL_APP_KEY_COMP_EVT
     */
    struct meshx_provisioner_update_local_app_key_comp_param
    {
        int err_code;                  /*!< Indicate the result of updating local AppKey by the Provisioner */
        uint16_t net_idx;              /*!< NetKey Index */
        uint16_t app_idx;              /*!< AppKey Index */
    } provisioner_update_app_key_comp; /*!< Event parameter of MESHX_PROVISIONER_UPDATE_LOCAL_APP_KEY_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT
     */
    struct meshx_provisioner_bind_local_mod_app_comp_param
    {
        int err_code;                         /*!< Indicate the result of binding AppKey with model by the Provisioner */
        uint16_t element_addr;                /*!< Element address */
        uint16_t app_idx;                     /*!< AppKey Index */
        uint16_t company_id;                  /*!< Company ID */
        uint16_t model_id;                    /*!< Model ID */
    } provisioner_bind_app_key_to_model_comp; /*!< Event parameter of MESHX_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_ADD_LOCAL_NET_KEY_COMP_EVT
     */
    struct meshx_provisioner_add_local_net_key_comp_param
    {
        int err_code;               /*!< Indicate the result of adding local NetKey by the Provisioner */
        uint16_t net_idx;           /*!< NetKey Index */
    } provisioner_add_net_key_comp; /*!< Event parameter of MESHX_PROVISIONER_ADD_LOCAL_NET_KEY_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_UPDATE_LOCAL_NET_KEY_COMP_EVT
     */
    struct meshx_provisioner_update_local_net_key_comp_param
    {
        int err_code;                  /*!< Indicate the result of updating local NetKey by the Provisioner */
        uint16_t net_idx;              /*!< NetKey Index */
    } provisioner_update_net_key_comp; /*!< Event parameter of MESHX_PROVISIONER_UPDATE_LOCAL_NET_KEY_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT
     */
    struct meshx_provisioner_store_node_comp_data_comp_param
    {
        int err_code;                        /*!< Indicate the result of storing node composition data by the Provisioner */
        uint16_t addr;                       /*!< Node element address */
    } provisioner_store_node_comp_data_comp; /*!< Event parameter of MESHX_PROVISIONER_STORE_NODE_COMP_DATA_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_DELETE_NODE_WITH_UUID_COMP_EVT
     */
    struct meshx_provisioner_delete_node_with_uuid_comp_param
    {
        int err_code;                         /*!< Indicate the result of deleting node with uuid by the Provisioner */
        uint8_t uuid[16];                     /*!< Node device uuid */
    } provisioner_delete_node_with_uuid_comp; /*!< Event parameter of MESHX_PROVISIONER_DELETE_NODE_WITH_UUID_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_DELETE_NODE_WITH_ADDR_COMP_EVT
     */
    struct meshx_provisioner_delete_node_with_addr_comp_param
    {
        int err_code;                         /*!< Indicate the result of deleting node with unicast address by the Provisioner */
        uint16_t unicast_addr;                /*!< Node unicast address */
    } provisioner_delete_node_with_addr_comp; /*!< Event parameter of MESHX_PROVISIONER_DELETE_NODE_WITH_ADDR_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_ENABLE_HEARTBEAT_RECV_COMP_EVT
     */
    struct
    {
        int err_code;                         /*!< Indicate the result of enabling/disabling to receive heartbeat messages by the Provisioner */
        bool enable;                          /*!< Indicate enabling or disabling receiving heartbeat messages */
    } provisioner_enable_heartbeat_recv_comp; /*!< Event parameters of MESHX_PROVISIONER_ENABLE_HEARTBEAT_RECV_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_TYPE_COMP_EVT
     */
    struct
    {
        int err_code;                             /*!< Indicate the result of setting the heartbeat filter type by the Provisioner */
        uint8_t type;                             /*!< Type of the filter used for receiving heartbeat messages */
    } provisioner_set_heartbeat_filter_type_comp; /*!< Event parameters of MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_TYPE_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_INFO_COMP_EVT
     */
    struct
    {
        int err_code;                             /*!< Indicate the result of setting the heartbeat filter address by the Provisioner */
        uint8_t op;                               /*!< Operation (add, remove, clean) */
        uint16_t hb_src;                          /*!< Heartbeat source address */
        uint16_t hb_dst;                          /*!< Heartbeat destination address */
    } provisioner_set_heartbeat_filter_info_comp; /*!< Event parameters of MESHX_PROVISIONER_SET_HEARTBEAT_FILTER_INFO_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_RECV_HEARTBEAT_MESSAGE_EVT
     */
    struct
    {
        uint16_t hb_src;          /*!< Heartbeat source address */
        uint16_t hb_dst;          /*!< Heartbeat destination address */
        uint8_t init_ttl;         /*!< Heartbeat InitTTL */
        uint8_t rx_ttl;           /*!< Heartbeat RxTTL */
        uint8_t hops;             /*!< Heartbeat hops (InitTTL - RxTTL + 1) */
        uint16_t feature;         /*!< Bit field of currently active features of the node */
        int8_t rssi;              /*!< RSSI of the heartbeat message */
    } provisioner_recv_heartbeat; /*!< Event parameters of MESHX_PROVISIONER_RECV_HEARTBEAT_MESSAGE_EVT */
    /**
     * @brief MESHX_PROVISIONER_DIRECT_ERASE_SETTINGS_COMP_EVT
     */
    struct
    {
        int err_code;                         /*!< Indicate the result of directly erasing settings by the Provisioner */
    } provisioner_direct_erase_settings_comp; /*!< Event parameters of MESHX_PROVISIONER_DIRECT_ERASE_SETTINGS_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_OPEN_SETTINGS_WITH_INDEX_COMP_EVT
     */
    struct
    {
        int err_code;                            /*!< Indicate the result of opening settings with index by the Provisioner */
        uint8_t index;                           /*!< Index of Provisioner settings */
    } provisioner_open_settings_with_index_comp; /*!< Event parameter of MESHX_PROVISIONER_OPEN_SETTINGS_WITH_INDEX_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_OPEN_SETTINGS_WITH_UID_COMP_EVT
     */
    struct
    {
        int err_code;                                 /*!< Indicate the result of opening settings with user id by the Provisioner */
        uint8_t index;                                /*!< Index of Provisioner settings */
        char uid[MESHX_SETTINGS_UID_SIZE + 1]; /*!< Provisioner settings user id */
    } provisioner_open_settings_with_uid_comp;        /*!< Event parameters of MESHX_PROVISIONER_OPEN_SETTINGS_WITH_UID_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_INDEX_COMP_EVT
     */
    struct
    {
        int err_code;                             /*!< Indicate the result of closing settings with index by the Provisioner */
        uint8_t index;                            /*!< Index of Provisioner settings */
    } provisioner_close_settings_with_index_comp; /*!< Event parameter of MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_INDEX_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_UID_COMP_EVT
     */
    struct
    {
        int err_code;                                 /*!< Indicate the result of closing settings with user id by the Provisioner */
        uint8_t index;                                /*!< Index of Provisioner settings */
        char uid[MESHX_SETTINGS_UID_SIZE + 1]; /*!< Provisioner settings user id */
    } provisioner_close_settings_with_uid_comp;       /*!< Event parameters of MESHX_PROVISIONER_CLOSE_SETTINGS_WITH_UID_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_DELETE_SETTINGS_WITH_INDEX_COMP_EVT
     */
    struct
    {
        int err_code;                              /*!< Indicate the result of deleting settings with index by the Provisioner */
        uint8_t index;                             /*!< Index of Provisioner settings */
    } provisioner_delete_settings_with_index_comp; /*!< Event parameter of MESHX_PROVISIONER_DELETE_SETTINGS_WITH_INDEX_COMP_EVT */
    /**
     * @brief MESHX_PROVISIONER_DELETE_SETTINGS_WITH_UID_COMP_EVT
     */
    struct
    {
        int err_code;                                 /*!< Indicate the result of deleting settings with user id by the Provisioner */
        uint8_t index;                                /*!< Index of Provisioner settings */
        char uid[MESHX_SETTINGS_UID_SIZE + 1]; /*!< Provisioner settings user id */
    } provisioner_delete_settings_with_uid_comp;      /*!< Event parameters of MESHX_PROVISIONER_DELETE_SETTINGS_WITH_UID_COMP_EVT */
    /**
     * @brief MESHX_SET_FAST_PROV_INFO_COMP_EVT
     */
    struct meshx_set_fast_prov_info_comp_param
    {
        uint8_t status_unicast; /*!< Indicate the result of setting unicast address range of fast provisioning */
        uint8_t status_net_idx; /*!< Indicate the result of setting NetKey Index of fast provisioning */
        uint8_t status_match;   /*!< Indicate the result of setting matching Device UUID of fast provisioning */
    } set_fast_prov_info_comp;  /*!< Event parameter of MESHX_SET_FAST_PROV_INFO_COMP_EVT */
    /**
     * @brief MESHX_SET_FAST_PROV_ACTION_COMP_EVT
     */
    struct meshx_set_fast_prov_action_comp_param
    {
        uint8_t status_action;   /*!< Indicate the result of setting action of fast provisioning */
    } set_fast_prov_action_comp; /*!< Event parameter of MESHX_SET_FAST_PROV_ACTION_COMP_EVT */
    /**
     * @brief MESHX_HEARTBEAT_MESSAGE_RECV_EVT
     */
    struct meshx_heartbeat_msg_recv_param
    {
        uint8_t hops;     /*!< Heartbeat hops (InitTTL - RxTTL + 1) */
        uint16_t feature; /*!< Bit field of currently active features of the node */
    } heartbeat_msg_recv; /*!< Event parameter of MESHX_HEARTBEAT_MESSAGE_RECV_EVT */
    /**
     * @brief MESHX_LPN_ENABLE_COMP_EVT
     */
    struct meshx_lpn_enable_comp_param
    {
        int err_code;  /*!< Indicate the result of enabling LPN functionality */
    } lpn_enable_comp; /*!< Event parameter of MESHX_LPN_ENABLE_COMP_EVT */
    /**
     * @brief MESHX_LPN_DISABLE_COMP_EVT
     */
    struct meshx_lpn_disable_comp_param
    {
        int err_code;   /*!< Indicate the result of disabling LPN functionality */
    } lpn_disable_comp; /*!< Event parameter of MESHX_LPN_DISABLE_COMP_EVT */
    /**
     * @brief MESHX_LPN_POLL_COMP_EVT
     */
    struct meshx_lpn_poll_comp_param
    {
        int err_code; /*!< Indicate the result of sending Friend Poll */
    } lpn_poll_comp;  /*!< Event parameter of MESHX_LPN_POLL_COMP_EVT */
    /**
     * @brief MESHX_LPN_FRIENDSHIP_ESTABLISH_EVT
     */
    struct meshx_lpn_friendship_establish_param
    {
        uint16_t friend_addr;   /*!< Friend Node unicast address */
    } lpn_friendship_establish; /*!< Event parameter of MESHX_LPN_FRIENDSHIP_ESTABLISH_EVT */
    /**
     * @brief MESHX_LPN_FRIENDSHIP_TERMINATE_EVT
     */
    struct meshx_lpn_friendship_terminate_param
    {
        uint16_t friend_addr;   /*!< Friend Node unicast address */
    } lpn_friendship_terminate; /*!< Event parameter of MESHX_LPN_FRIENDSHIP_TERMINATE_EVT */
    /**
     * @brief MESHX_FRIEND_FRIENDSHIP_ESTABLISH_EVT
     */
    struct meshx_friend_friendship_establish_param
    {
        uint16_t lpn_addr;       /*!< Low Power Node unicast address */
    } frnd_friendship_establish; /*!< Event parameter of MESHX_FRIEND_FRIENDSHIP_ESTABLISH_EVT */
    /**
     * @brief MESHX_FRIEND_FRIENDSHIP_TERMINATE_EVT
     */
    struct meshx_friend_friendship_terminate_param
    {
        uint16_t lpn_addr; /*!< Low Power Node unicast address */
        /** This enum value is the reason of friendship termination on the friend node side */
        enum
        {
            MESHX_FRND_FRIENDSHIP_TERMINATE_ESTABLISH_FAIL,  /*!< Friend Offer has been sent, but Friend Offer is not received within 1 second, friendship fails to be established */
            MESHX_FRND_FRIENDSHIP_TERMINATE_POLL_TIMEOUT,    /*!< Friendship is established, PollTimeout timer expires and no Friend Poll/Sub Add/Sub Remove is received */
            MESHX_FRND_FRIENDSHIP_TERMINATE_RECV_FRND_REQ,   /*!< Receive Friend Request from existing Low Power Node */
            MESHX_FRND_FRIENDSHIP_TERMINATE_RECV_FRND_CLEAR, /*!< Receive Friend Clear from other friend node */
            MESHX_FRND_FRIENDSHIP_TERMINATE_DISABLE,         /*!< Friend feature disabled or corresponding NetKey is deleted */
        } reason;                                                   /*!< Friendship terminated reason */
    } frnd_friendship_terminate;                                    /*!< Event parameter of MESHX_FRIEND_FRIENDSHIP_TERMINATE_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_RECV_ADV_PKT_EVT
     */
    struct meshx_proxy_client_recv_adv_pkt_param
    {
        meshx_bd_addr_t addr;        /*!< Device address */
        meshx_addr_type_t addr_type; /*!< Device address type */
        uint16_t net_idx;            /*!< Network ID related NetKey Index */
        uint8_t net_id[8];           /*!< Network ID contained in the advertising packet */
        int8_t rssi;                 /*!< RSSI of the received advertising packet */
    } proxy_client_recv_adv_pkt;     /*!< Event parameter of MESHX_PROXY_CLIENT_RECV_ADV_PKT_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_CONNECTED_EVT
     */
    struct meshx_proxy_client_connected_param
    {
        meshx_bd_addr_t addr;        /*!< Device address of the Proxy Server */
        meshx_addr_type_t addr_type; /*!< Device address type */
        uint8_t conn_handle;         /*!< Proxy connection handle */
        uint16_t net_idx;            /*!< Corresponding NetKey Index */
    } proxy_client_connected;        /*!< Event parameter of MESHX_PROXY_CLIENT_CONNECTED_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_DISCONNECTED_EVT
     */
    struct meshx_proxy_client_disconnected_param
    {
        meshx_bd_addr_t addr;        /*!< Device address of the Proxy Server */
        meshx_addr_type_t addr_type; /*!< Device address type */
        uint8_t conn_handle;         /*!< Proxy connection handle */
        uint16_t net_idx;            /*!< Corresponding NetKey Index */
        uint8_t reason;              /*!< Proxy disconnect reason */
    } proxy_client_disconnected;     /*!< Event parameter of MESHX_PROXY_CLIENT_DISCONNECTED_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_RECV_FILTER_STATUS_EVT
     */
    struct meshx_proxy_client_recv_filter_status_param
    {
        uint8_t conn_handle;           /*!< Proxy connection handle */
        uint16_t server_addr;          /*!< Proxy Server primary element address */
        uint16_t net_idx;              /*!< Corresponding NetKey Index */
        uint8_t filter_type;           /*!< Proxy Server filter type(whitelist or blacklist) */
        uint16_t list_size;            /*!< Number of addresses in the Proxy Server filter list */
    } proxy_client_recv_filter_status; /*!< Event parameter of MESHX_PROXY_CLIENT_RECV_FILTER_STATUS_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_CONNECT_COMP_EVT
     */
    struct meshx_proxy_client_connect_comp_param
    {
        int err_code;                /*!< Indicate the result of Proxy Client connect */
        meshx_bd_addr_t addr;        /*!< Device address of the Proxy Server */
        meshx_addr_type_t addr_type; /*!< Device address type */
        uint16_t net_idx;            /*!< Corresponding NetKey Index */
    } proxy_client_connect_comp;     /*!< Event parameter of MESHX_PROXY_CLIENT_CONNECT_COMP_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_DISCONNECT_COMP_EVT
     */
    struct meshx_proxy_client_disconnect_comp_param
    {
        int err_code;               /*!< Indicate the result of Proxy Client disconnect */
        uint8_t conn_handle;        /*!< Proxy connection handle */
    } proxy_client_disconnect_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_DISCONNECT_COMP_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_SET_FILTER_TYPE_COMP_EVT
     */
    struct meshx_proxy_client_set_filter_type_comp_param
    {
        int err_code;                    /*!< Indicate the result of Proxy Client set filter type */
        uint8_t conn_handle;             /*!< Proxy connection handle */
        uint16_t net_idx;                /*!< Corresponding NetKey Index */
    } proxy_client_set_filter_type_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_SET_FILTER_TYPE_COMP_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_ADD_FILTER_ADDR_COMP_EVT
     */
    struct meshx_proxy_client_add_filter_addr_comp_param
    {
        int err_code;                    /*!< Indicate the result of Proxy Client add filter address */
        uint8_t conn_handle;             /*!< Proxy connection handle */
        uint16_t net_idx;                /*!< Corresponding NetKey Index */
    } proxy_client_add_filter_addr_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_ADD_FILTER_ADDR_COMP_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_REMOVE_FILTER_ADDR_COMP_EVT
     */
    struct meshx_proxy_client_remove_filter_addr_comp_param
    {
        int err_code;                       /*!< Indicate the result of Proxy Client remove filter address */
        uint8_t conn_handle;                /*!< Proxy connection handle */
        uint16_t net_idx;                   /*!< Corresponding NetKey Index */
    } proxy_client_remove_filter_addr_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_REMOVE_FILTER_ADDR_COMP_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_DIRECTED_PROXY_CONTROL_COMP_EVT
     */
    struct meshx_proxy_client_directed_proxy_set_param
    {
        int err_code;                       /*!< Indicate the result of Proxy Client directed proxy control address */
        uint8_t conn_handle;                /*!< Proxy connection handle */
        uint16_t net_idx;                   /*!< Corresponding NetKey Index */
    } proxy_client_directed_proxy_set_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_DIRECTED_PROXY_SET_COMP_EVT */
    /**
     * @brief MESHX_PROXY_SERVER_CONNECTED_EVT
     */
    struct meshx_proxy_server_connected_param
    {
        uint8_t conn_handle;  /*!< Proxy connection handle */
    } proxy_server_connected; /*!< Event parameter of MESHX_PROXY_SERVER_CONNECTED_EVT */
    /**
     * @brief MESHX_PROXY_SERVER_DISCONNECTED_EVT
     */
    struct meshx_proxy_server_disconnected_param
    {
        uint8_t conn_handle;     /*!< Proxy connection handle */
        uint8_t reason;          /*!< Proxy disconnect reason */
    } proxy_server_disconnected; /*!< Event parameter of MESHX_PROXY_SERVER_DISCONNECTED_EVT */
    /**
     * @brief MESHX_PROXY_CLIENT_SEND_SOLIC_PDU_COMP_EVT
     */
    struct
    {
        int err_code;                   /*!< Indicate the result of Proxy Client send Solicitation PDU */
        uint16_t net_idx;               /*!< Corresponding NetKey Index */
        uint16_t ssrc;                  /*!< Solicitation SRC */
        uint16_t dst;                   /*!< Solicitation DST */
    } proxy_client_send_solic_pdu_comp; /*!< Event parameter of MESHX_PROXY_CLIENT_SEND_SOLIC_PDU_COMP_EVT */
    /**
     * @brief MESHX_MODEL_SUBSCRIBE_GROUP_ADDR_COMP_EVT
     */
    struct meshx_model_sub_group_addr_comp_param
    {
        int err_code;            /*!< Indicate the result of local model subscribing group address */
        uint16_t element_addr;   /*!< Element address */
        uint16_t company_id;     /*!< Company ID */
        uint16_t model_id;       /*!< Model ID */
        uint16_t group_addr;     /*!< Group Address */
    } model_sub_group_addr_comp; /*!< Event parameters of MESHX_MODEL_SUBSCRIBE_GROUP_ADDR_COMP_EVT */
    /**
     * @brief MESHX_MODEL_UNSUBSCRIBE_GROUP_ADDR_COMP_EVT
     */
    struct meshx_model_unsub_group_addr_comp_param
    {
        int err_code;              /*!< Indicate the result of local model unsubscribing group address */
        uint16_t element_addr;     /*!< Element address */
        uint16_t company_id;       /*!< Company ID */
        uint16_t model_id;         /*!< Model ID */
        uint16_t group_addr;       /*!< Group Address */
    } model_unsub_group_addr_comp; /*!< Event parameters of MESHX_MODEL_UNSUBSCRIBE_GROUP_ADDR_COMP_EVT */
    /**
     * @brief MESHX_DEINIT_MESH_COMP_EVT
     */
    struct meshx_deinit_mesh_comp_param
    {
        int err_code;    /*!< Indicate the result of BLE Mesh deinitialization */
    } deinit_mesh_comp;  /*!< Event parameter of MESHX_DEINIT_MESH_COMP_EVT */
} meshx_prov_cb_param_t; /*!< Event parameters of MESHX_DEINIT_MESH_COMP_EVT */

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param uuid Pointer to the UUID of the device.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_plat_init_prov(const uint8_t *uuid);

/**
 * @brief Get the provisioning parameters.
 *
 * This function returns a pointer to the global provisioning parameters.
 *
 * @return Pointer to the global provisioning parameters.
 */
MESHX_PROV *meshx_plat_get_prov(void);

#endif /* __MESHX_BLE_MESH_PROV_SRV_H__ */
