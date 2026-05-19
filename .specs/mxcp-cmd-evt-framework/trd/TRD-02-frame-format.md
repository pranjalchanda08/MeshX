# TRD-02 — MXCP Frame Format and State Machine

## 1. Frame Structure

The MXCP frame retains the same structure as MXSP — only the semantics of the TYPE byte change. No new states are needed in the RX state machine.

```
 Byte 0:    SOF = 0xFE
 Byte 1:    LEN (payload length, 0..255)
 Byte 2:    TYPE (8-bit):
              Bit 7:     Direction (0 = CMD, 1 = EVT)
              Bits 6-0:  Command/Event ID (0x00..0x7F)
 Byte 3..N: PAYLOAD (typed struct, up to 255 bytes)
 Byte N+1:  CHECKSUM (XOR of LEN ^ TYPE ^ all payload bytes)
 Byte N+2:  EOF = 0xEF
```

## 2. Frame Header Bit Field

```c
#define MXCP_TYPE_DIR_CMD  0x00
#define MXCP_TYPE_DIR_EVT  0x80
#define MXCP_TYPE_ID_MASK  0x7F

#define MXCP_MAKE_TYPE(dir, id)   ((uint8_t)((dir) | ((id) & MXCP_TYPE_ID_MASK)))
#define MXCP_TYPE_IS_CMD(t)       (((t) & 0x80) == 0)
#define MXCP_TYPE_IS_EVT(t)       (((t) & 0x80) != 0)
#define MXCP_TYPE_ID(t)           ((t) & MXCP_TYPE_ID_MASK)
```

## 3. Frame C Structure

```c
#define MXCP_SOF 0xFE
#define MXCP_EOF 0xEF
#define MXCP_PAYLOAD_MAX_SIZE 255

typedef struct {
    uint8_t sof;
    uint8_t len;
    uint8_t type;        /* DIR (bit 7) + ID (bits 6-0) */
    uint8_t payload[MXCP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
    uint8_t eof;
} mxcp_frame_t;
```

## 4. Migration Note

The frame structure is identical to the existing MXSP frame — `[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`. The only change is that the TYPE byte now encodes direction in bit 7 and a flat command/event ID in bits 6-0. The RX state machine requires **no new states**. *(REQ-003)*

## 5. UART RX State Machine

### 5.1 State Machine — No Structural Changes

The state machine remains identical to the current MXSP parser. The TYPE byte (8-bit) is parsed in the existing `STATE_TYPE` state — no new states needed.

```c
enum {
    STATE_SOF,
    STATE_LEN,
    STATE_TYPE,      /* Unchanged: parses 1-byte TYPE */
    STATE_PAYLOAD,
    STATE_CHECKSUM,
    STATE_EOF
};
```

### 5.2 Checksum — No Change

Checksum calculation remains: `LEN ^ TYPE ^ (payload bytes...)`.
