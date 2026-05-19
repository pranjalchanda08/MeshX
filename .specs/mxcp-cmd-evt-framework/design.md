# MXCP Framework — Technical Design (Master Index)

This directory contains the technical design split into focused TRD documents.

| TRD | Title | Sections |
|-----|-------|----------|
| [TRD-01](trd/TRD-01-architecture.md) | System Context and Architecture | Overview, architectural diagram, key changes |
| [TRD-02](trd/TRD-02-frame-format.md) | Frame Format and State Machine | Frame structure, header bit field, C struct, RX state machine, checksum |
| [TRD-03](trd/TRD-03-id-namespace.md) | Command and Event ID Namespace | CMD IDs (bit 7=0), EVT IDs (bit 7=1) |
| [TRD-04](trd/TRD-04-payload-structures.md) | Typed Payload Structures | System CMD/Event payloads, GPIO CMD/Event payloads |
| [TRD-05](trd/TRD-05-tables-dispatch-api.md) | Tables, Dispatch, and TX API | Command table, event table, single-layer dispatch, unified TX API |
| [TRD-06](trd/TRD-06-migration-mapping.md) | Handler Migration and File Structure | Old→New handler mapping, new/modified/removed files |
| [TRD-07](trd/TRD-07-decisions-dependencies.md) | Design Decisions and Dependencies | Trade-offs, internal/external dependencies |
| [TRD-08](trd/TRD-08-host-tooling-migration.md) | Host-Side Tooling Migration | `demux.py`, `server.py` changes, TYPE value mapping |
| [TRD-09](trd/TRD-09-sequence-diagrams.md) | Sequence Diagrams | Sync CMD/RSP, async CMD/EVT, unsolicited EVT, element data flow, layer diagram |
