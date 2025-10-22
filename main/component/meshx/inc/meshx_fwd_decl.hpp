/**
 * @file meshx_fwd_decl.hpp
 * @brief Forward declaration of MeshX classes
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 * @author Pranjal Chanda
 */

#ifndef __MESHX_FWD_DEC_H__
#define __MESHX_FWD_DEC_H__

/***************************************************************************************************************************************
 * Includes
 ***************************************************************************************************************************************/
#include <memory>
#include <utility>
#include <functional>
#include <forward_list>
#include <meshx_c_header.h>

/***************************************************************************************************************************************
 * Template Prototypes
 ***************************************************************************************************************************************/
#define MESHX_BASE_TEMPLATE_PROTO          template <typename ble_mesh_send_msg_params_t>
#define MESHX_BASE_TEMPLATE_PARAMS                  <ble_mesh_send_msg_params_t>
#define MESHX_BASE_CLIENT_TEMPLATE_PROTO   template <typename baseClientModelDerived_t, typename ble_mesh_send_msg_params_t, typename ble_mesh_plat_model_cb_params_t>
#define MESHX_BASE_CLIENT_TEMPLATE_PARAMS           <baseClientModelDerived_t, ble_mesh_send_msg_params_t, ble_mesh_plat_model_cb_params_t>
#define MESHX_BASE_SERVER_TEMPLATE_PROTO   template <typename baseServerModelDerived_t, typename ble_mesh_send_msg_params_t>
#define MESHX_BASE_SERVER_TEMPLATE_PARAMS           <baseServerModelDerived_t, ble_mesh_send_msg_params_t>

#define MESHX_MODEL_TEMPLATE_PROTO          template <typename meshxBaseModel_t, typename meshx_send_packet_params_t>
#define MESHX_MODEL_TEMPLATE_PARAMS                  <meshxBaseModel_t, meshx_send_packet_params_t>
#define MESHX_SERVER_MODEL_TEMPLATE_PROTO   template <typename meshxBaseServerModel_t, typename meshx_send_packet_params_t>
#define MESHX_SERVER_MODEL_TEMPLATE_PARAMS           <meshxBaseServerModel_t, meshx_send_packet_params_t>
#define MESHX_CLIENT_MODEL_TEMPLATE_PROTO   template <typename meshxBaseClientModel_t, typename meshx_send_packet_params_t>
#define MESHX_CLIENT_MODEL_TEMPLATE_PARAMS           <meshxBaseClientModel_t, meshx_send_packet_params_t>

#define MESHX_ELEMENT_TEMPLATE_PROTO
#define MESHX_ELEMENT_TEMPLATE_PARAMS

/***************************************************************************************************************************************
 * Forward declaration of Classes
 ***************************************************************************************************************************************/

MESHX_BASE_TEMPLATE_PROTO class meshXBaseModel;
MESHX_BASE_SERVER_TEMPLATE_PROTO class meshXBaseServerModel;
MESHX_BASE_CLIENT_TEMPLATE_PROTO class meshXBaseClientModel;

MESHX_MODEL_TEMPLATE_PROTO class meshXModel;
MESHX_SERVER_MODEL_TEMPLATE_PROTO class meshXServerModel;
MESHX_CLIENT_MODEL_TEMPLATE_PROTO class meshXClientModel;

MESHX_ELEMENT_TEMPLATE_PROTO    class meshXElement;

#endif/* __MESHX_FWD_DEC_H__ */
