/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_unpair_mgr.h
 * @brief Autonomous Unpair Manager and Pre-Reset Lifecycle Teardown.
 *
 * Manages granular element unpairing, zero-reference group reclamation,
 * offline peer self-healing, and mandatory pre-reset teardown handshakes.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#ifndef __MESHX_UNPAIR_MGR_H__
#define __MESHX_UNPAIR_MGR_H__

#include <stdint.h>
#include <stdbool.h>
#include <meshx_common.h>
#include <meshx_uvp.h>
#include <pairing/meshx_manifest.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for asynchronous unpair completion.
 *
 * @param[in] status     Result of unpairing.
 * @param[in] peer_addr  Unicast address of peer node.
 * @param[in] peer_elem  Element index on peer node.
 */
typedef void (*meshx_unpair_status_cb_t)(meshx_err_t status,
                                         uint16_t    peer_addr,
                                         uint8_t     peer_elem);

/**
 * @brief Initialize the unpair manager subsystem.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_init(void);

/**
 * @brief Register a callback for unpair events.
 *
 * @param[in] cb Callback function.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_register_cb(meshx_unpair_status_cb_t cb);

/**
 * @brief Granularly unpair an element link with a specific peer.
 *
 * Dispatches MESHX_SYS_CMD_UNPAIR_REQUEST to peer, resets publication target
 * or removes group subscription if reference count reaches 0, and updates NVS.
 *
 * @param[in] src_elem_idx Local element index.
 * @param[in] peer_unicast Unicast address of peer node.
 * @param[in] peer_elem    Element index on peer node.
 * @param[in] func_id      Functional capability ID.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_element(uint8_t  src_elem_idx,
                                 uint16_t peer_unicast,
                                 uint8_t  peer_elem,
                                 uint16_t func_id);

/**
 * @brief Handle an incoming MESHX_SYS_CMD_UNPAIR_REQUEST wire frame.
 *
 * @param[in]  req         Received unpair request payload.
 * @param[in]  src_unicast Unicast address of requesting node.
 * @param[out] rsp         Response payload to transmit back.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_handle_request(const meshx_uvp_unpair_req_t *req,
                                        uint16_t                      src_unicast,
                                        meshx_uvp_unpair_rsp_t       *rsp);

/**
 * @brief Handle an incoming MESHX_SYS_RSP_UNPAIR_RESPONSE wire frame.
 *
 * @param[in] rsp         Received unpair response.
 * @param[in] src_unicast Unicast address of respondent node.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_handle_response(const meshx_uvp_unpair_rsp_t *rsp,
                                         uint16_t                      src_unicast);

/**
 * @brief Handle a received MESHX_SYS_NOTIFY_NODE_RESET broadcast frame.
 *
 * Cleanses all active links associated with the resetting peer, reclaims
 * unreferenced groups, and updates NVS.
 *
 * @param[in] reset_node_addr Unicast address of the resetting node.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_handle_reset_notify(uint16_t reset_node_addr);

/**
 * @brief Execute mandatory Pre-Reset Lifecycle Teardown.
 *
 * Transmits targeted unpair requests to all active peers, broadcasts
 * MESHX_SYS_NOTIFY_NODE_RESET to the Company Management Virtual Address,
 * waits for a 500ms network drain delay, and erases local link NVS.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_unpair_pre_reset_teardown(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UNPAIR_MGR_H__ */
