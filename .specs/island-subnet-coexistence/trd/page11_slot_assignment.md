# TRD Page 11: Subnet Slot ID Assignment & Registry Architecture

**Module:** `TRD-SUBNET-11`
**Parent Index:** [TRD Index](./README.md)
**Requirements:** [requirements.md](../requirements.md) (REQ-F01, REQ-F03)
**Related Fixes:** Remediation of B, C findings from `REVIEW_ROUND2_FINDINGS.md`

---

## 1. The Local Scoping Principle

> **A `slot_id` (0..13) is a LOCAL interface index within an island, NOT a global identifier across the entire universe.**

- **Global Subnet ID (`global_subnet_id`)**: Globally unique 16-bit identifier derived at genesis (`CRC16(Anchor_UUID || net_key_epoch_0[0..3])`).
- **Slot ID (`slot_id`)**: A local index (`0..13`) used by an island to map a foreign island into its Tier-1 virtual address window (`0x4000..0x77FF`).

---

## 2. Slot Registry Allocation & Exhaustion Handling

Every island maintains a bounded local **Subnet Slot Registry** using a **dynamically allocated singly-linked list** (`meshx_platform_malloc` / `meshx_platform_free`, zero `realloc` copies):

```c
#define MESHX_SLOT_MAX 14

/* Natural field alignment on 32-bit MCU: pointer -> uint32_t -> uint16_t -> uint8_t = 13 B packed */
typedef struct meshx_local_slot_entry {
    struct meshx_local_slot_entry *next; /* Singly-linked list pointer (4 B on 32-bit MCU) */
    uint32_t allocated_ts_s;             /* Allocation monotonic timestamp (s) */
    uint16_t remote_global_subnet;       /* e.g., 0xC03A (Island C) */
    uint16_t proxy_base_addr;            /* 0x4000 + (slot_id << 10) */
    uint8_t  slot_id;                    /* 0 .. 13 */
} MESHX_PACKED meshx_local_slot_entry_t;

/* Registry control head in BSS (12 B packed; nodes allocated individually per active subnet) */
typedef struct {
    meshx_local_slot_entry_t *head;      /* Pointer to head of slot list (NULL when unbridged) */
    meshx_local_slot_entry_t *tail;      /* Pointer to tail of slot list */
    uint16_t coordinator_addr;           /* Elected Subnet Slot Coordinator unicast address */
    uint8_t  next_free_slot;             /* Monotonic count / next index: 0 .. 14 (overflow at 14) */
    uint8_t  overflow_active;            /* 1 if Tier-2 dynamic pool engaged */
} MESHX_PACKED meshx_local_slot_registry_t;
```

### 2.1 Dynamic Linked-List Lifecycle, Bounded Allocation & Tier-2 Fallback
- **Zero-Overhead Idle & Per-Island Node Allocation**: When an island has 0 foreign peers (`head == tail == NULL`), the registry consumes **0 B heap**. Each newly bridged foreign subnet allocates one `meshx_local_slot_entry_t` (`13 B` packed + `~8 B` RTOS heap header = `21 B`) via `meshx_platform_malloc(sizeof(meshx_local_slot_entry_t))` and appends to `tail` under mutex — zero `realloc` copies and stable node pointers (matching the lifecycle pattern of [`meshx_router_queue_t`](file:///d:/MeshX/.specs/island-subnet-coexistence/trd/page4_store_forward_router.md#L186-L216)).
- **Bounded Overflow into Tier-2**: `next_free_slot` is strictly bounded to indices `0 .. 13` (values `>= 14` never allocate beyond 14 nodes or execute bit shifts on `slot_id`). When an island connects to a 15th foreign island (`next_free_slot >= 14`), it activates `overflow_active = 1` and steers subsequent foreign elements through the **Tier-2 Dynamic Alias Pool (`0x7800..0x7FFF`)**.
- **Atomic Unlink, Scrub & Free**: Removing a decommissioned slot or resetting the registry traverses the list from `head`, unlinks atomically, zeroes each node (`memset(node, 0, sizeof(*node))`), executes `meshx_platform_free(node)`, and resets `head = tail = NULL`, `next_free_slot = overflow_active = 0`. Note that NVS persistence serializes only the 9 B data payload per slot (excluding `*next`).

---

## 3. Single-Coordinator Slot Allocation & Digest Consensus

When multiple active boundary bridges coexist on the same subnet (e.g., `Bridge A-B` and `Bridge B-C` both operating on Island B), uncoordinated simultaneous slot assignment would cause both bridges to claim `Slot 0` for different remote islands:
1. **Deterministic Slot Coordinator Election**: On any shared subnet, the active bridge elected by highest score (`Anchor 0x0001` if bridge-capable, else lowest `bridge_unicast_addr` among active bridges on that subnet) serves as the authoritative Slot Coordinator for that island. Peer bridges propose new `(global_subnet_id)` additions to the Coordinator rather than self-assigning overlapping slot indices.
2. **Digest-Only Heartbeats**: To keep `4.0s` `0xD1` heartbeats compact (non-fragmenting), `0xD1` carries a **32-bit `slot_registry_crc32`** + `(kr_phase, current_epoch)` summary rather than the full 14-entry registry table. Full/delta registry sync runs on demand strictly when a digest mismatch or new bridge join is detected.
