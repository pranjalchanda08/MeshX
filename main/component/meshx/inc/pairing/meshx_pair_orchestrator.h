/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_pair_orchestrator.h
 * @brief Autonomous Pairing Orchestration Engine interface.
 *
 * Handles end-to-end element pairing transactions, topology planning (1:1 unicast
 * vs 1:N group addressing), dynamic topology promotion, and link table updates.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#ifndef __MESHX_PAIR_ORCHESTRATOR_H__
#define __MESHX_PAIR_ORCHESTRATOR_H__

#include <stdint.h>
#include <stdbool.h>
#include <meshx_common.h>
#include <meshx_uvp.h>
#include <pairing/meshx_manifest.h>
#include <pairing/meshx_compat_matrix.h>
#include <pairing/meshx_group_allocator.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for asynchronous pairing completion.
 *
 * @param[in] status     Result of the pairing transaction.
 * @param[in] peer_addr  Unicast address of peer node.
 * @param[in] peer_elem  Element index on peer node.
 * @param[in] group_addr Group address assigned (0x0000 for direct unicast).
 */
typedef void (*meshx_pair_status_cb_t)(meshx_err_t status,
                                       uint16_t    peer_addr,
                                       uint8_t     peer_elem,
                                       uint16_t    group_addr);

/**
 * @brief Initialize the pairing orchestrator engine.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_pair_init(void);

/**
 * @brief Register a callback for pairing transaction events.
 *
 * @param[in] cb Callback function.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_pair_register_cb(meshx_pair_status_cb_t cb);

/**
 * @brief Initiate an autonomous pairing transaction with a peer element.
 *
 * Evaluates existing topology for src_elem_idx. If an existing 1:1 link is present
 * on func_id, initiates Dynamic Topology Promotion (MESHX_SYS_CMD_TOPOLOGY_UPGRADE)
 * before dispatching MESHX_SYS_CMD_PAIR_REQUEST to the new peer.
 *
 * @param[in] src_elem_idx Local element index initiating the pairing.
 * @param[in] dst_unicast  Target node primary unicast address.
 * @param[in] dst_elem_idx Target element index on peer node.
 * @param[in] func_id      Functional capability ID to bind.
 *
 * @return MESHX_SUCCESS on initiation success, error code otherwise.
 */
meshx_err_t meshx_pair_initiate(uint8_t  src_elem_idx,
                                uint16_t dst_unicast,
                                uint8_t  dst_elem_idx,
                                uint16_t func_id);

/**
 * @brief Handle an incoming MESHX_SYS_CMD_PAIR_REQUEST wire frame.
 *
 * Validates semantic compatibility, provisions group subscription on local
 * actuator elements, persists the link record to NVS, and increments link_epoch.
 *
 * @param[in]  req         Received pair request payload.
 * @param[in]  src_unicast Unicast address of sender node.
 * @param[out] rsp         Response payload to transmit back to initiator.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_pair_handle_request(const meshx_uvp_pair_req_t *req,
                                      uint16_t                    src_unicast,
                                      meshx_uvp_pair_rsp_t       *rsp);

/**
 * @brief Handle an incoming MESHX_SYS_RSP_PAIR_RESPONSE wire frame.
 *
 * Configures the local model publication address, commits the binding to NVS,
 * increments link_epoch, and invokes registered completion callbacks.
 *
 * @param[in] rsp         Received pair response payload.
 * @param[in] src_unicast Unicast address of respondent node.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_pair_handle_response(const meshx_uvp_pair_rsp_t *rsp,
                                       uint16_t                    src_unicast);

/**
 * @brief Handle an incoming MESHX_SYS_CMD_TOPOLOGY_UPGRADE wire frame.
 *
 * Invoked on existing 1:1 peers when a producer promotes to a 1:N group cluster.
 * Subscribes to the new group address and updates the local link record.
 *
 * @param[in]  upgrade     Received upgrade request payload.
 * @param[in]  src_unicast Unicast address of initiator node.
 * @param[out] rsp         Response acknowledgement to transmit.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_pair_handle_upgrade(const meshx_uvp_upgrade_req_t *upgrade,
                                      uint16_t                       src_unicast,
                                      meshx_uvp_upgrade_rsp_t       *rsp);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_PAIR_ORCHESTRATOR_H__ */
