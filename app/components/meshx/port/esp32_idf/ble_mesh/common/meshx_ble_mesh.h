#ifndef __MESHX_BLE_MESH__
#define __MESHX_BLE_MESH__

#include "stdio.h"
#include "stdint.h"

typedef struct meshx_element meshx_element_t;

typedef struct meshx_model meshx_model_t;

struct meshx_element{
    /** Element Address, assigned during provisioning. */
    uint16_t element_addr;

    /** Location Descriptor (GATT Bluetooth Namespace Descriptors) */
    const uint16_t location;

    const uint8_t sig_model_count;      /*!< SIG Model count */
    const uint8_t vnd_model_count;      /*!< Vendor Model count */

    meshx_model_t *sig_models;   /*!< SIG Models */
    meshx_model_t *vnd_models;   /*!< Vendor Models */
};

typedef struct {
    /** Pointer to the model to which the context belongs. Initialized by the stack. */
    meshx_model_t *model;

    uint16_t publish_addr;  /*!< Publish Address. */
    uint16_t app_idx:12;    /*!< Publish AppKey Index. */
    uint16_t cred:1;        /*!< Friendship Credentials Flag. */
    uint16_t send_rel:1;    /*!< Force reliable sending (segment acks) */
    uint16_t send_szmic:1;  /*!< Size of TransMIC when publishing a Segmented Access message */

    uint8_t  ttl;           /*!< Publish Time to Live. */
    uint8_t  retransmit;    /*!< Retransmit Count & Interval Steps. */

    uint8_t  period;        /*!< Publish Period. */
    uint8_t  period_div:4;  /*!< Divisor for the Period. */
    uint8_t  fast_period:1; /*!< Use FastPeriodDivisor */
    uint8_t  count:3;       /*!< Retransmissions left. */

    uint32_t period_start;  /*!< Start of the current period. */

#if CONFIG_BLE_MESH_DF_SRV
    uint8_t  directed_pub_policy; /*!< Directed publish policy */
#endif

    /** @brief Publication buffer, containing the publication message.
     *
     *  This will get correctly created when the publication context
     *  has been defined using the ESP_BLE_MESH_MODEL_PUB_DEFINE macro.
     *
     *  ESP_BLE_MESH_MODEL_PUB_DEFINE(name, size);
     */
    struct net_buf_simple *msg;

    /** Callback used to update publish message. Initialized by the stack. */
    esp_ble_mesh_cb_t update;

    /** Publish Period Timer. Initialized by the stack. */
    struct k_delayed_work timer;

    /** Role of the device that is going to publish messages */
    uint8_t dev_role __attribute__((deprecated));
} esp_ble_mesh_model_pub_t;

struct meshx_model
{
    /** Model ID */
    union {
        const uint16_t model_id; /*!< 16-bit model identifier */
        struct {
            uint16_t company_id; /*!< 16-bit company identifier */
            uint16_t model_id;   /*!< 16-bit model identifier */
        } vnd; /*!< Structure encapsulating a model ID with a company ID */
    };

    /** Internal information, mainly for persistent storage */
    uint8_t  element_idx;   /*!< Belongs to Nth element */
    uint8_t  model_idx;     /*!< Is the Nth model in the element */
    uint16_t flags;         /*!< Information about what has changed */

    /** The Element to which this Model belongs */
    meshx_element_t *element;

    /** Model Publication */
    esp_ble_mesh_model_pub_t *const pub;

    /** AppKey List */
    uint16_t keys[CONFIG_BLE_MESH_MODEL_KEY_COUNT];

    /** Subscription List (group or virtual addresses) */
    uint16_t groups[CONFIG_BLE_MESH_MODEL_GROUP_COUNT];

    /** Model operation context */
    esp_ble_mesh_model_op_t *op;

    /** Model callback structure */
    esp_ble_mesh_model_cbs_t *cb;

    /** Model-specific user data */
    void *user_data;
};

struct meshx_element
{
    /** Element Address, assigned during provisioning. */
    uint16_t element_addr;

    /** Location Descriptor (GATT Bluetooth Namespace Descriptors) */
    uint16_t location;

    uint8_t sig_model_count; /*!< SIG Model count */
    uint8_t vnd_model_count; /*!< Vendor Model count */

    meshx_model_t *sig_models; /*!< SIG Models */
    meshx_model_t *vnd_models; /*!< Vendor Models */
};

typedef struct meshx_composition
{
    uint16_t cid; /*!< 16-bit SIG-assigned company identifier */
    uint16_t pid; /*!< 16-bit vendor-assigned product identifier */
    uint16_t vid; /*!< 16-bit vendor-assigned product version identifier */

    size_t element_count;      /*!< Element count */
    meshx_element_t *elements; /*!< A sequence of elements */
} meshx_composition_t;

#endif /* __MESHX_BLE_MESH__ */
