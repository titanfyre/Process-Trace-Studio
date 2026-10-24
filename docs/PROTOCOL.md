# Protocol and data ownership

`SDK/ProcessTrace.h` contains the pointer-free version-1 wire structures and the small deterministic ring/request/name helpers shared with the private kernel implementation. `ProcessTrace.cpp` is the user-mode SDK. Do not change packing or use a different wire version silently.

| Structure | Bytes | Use |
|---|---:|---|
| `PT_REQUEST` | 32 | Version, exact structure size, allowed Start options and five zero reserved values |
| `PT_EVENT` | 584 | Timestamp, identity, event-specific fields, flags and 260 UTF-16 code units |
| `PT_STATS` | 88 | Capture state, session, clock anchors and bounded-ring counters |
| `PT_BATCH` | 9448 | Status snapshot, up to 16 events, zero reserved/unused output bytes |

All four IOCTLs require read and write access to the secured device. Input/output sizes and flags are exact. Unknown IOCTLs, reserved bits and incompatible versions are rejected. The kernel consumes a private request copy before sharing a buffered output area. There are no user pointers in the protocol.

`Start` replaces the previous capture, resets its queued events/counters and increments the session. `Stop` cancels a pending reader and discards queued events. `Status` is immediate. `Receive` drains a bounded batch or waits through a KMDF-owned cancel-safe manual queue. Only one pending reader is admitted. A completion that wins a timeout/cancel race is returned as a real batch rather than discarded. Clients must consume or explicitly discard each returned batch.

The single device owner is a file object. File cleanup marks it closing, stops capture, cancels pending reads and releases ownership. A passive lifecycle lock serializes cleanup and control transitions. The Start commit additionally checks the owner, closing flag and device-removal state while holding the ring lock. An old in-flight callback cannot publish into a different session. Unload/removal disables collection, unregisters callbacks and waits for rundown before releasing context storage.

Process identity is `(boot, process_key, create_time_100ns)`; include the capture session when combining streams from driver reloads. PID alone is insufficient. `parent_key` is an **observed** lookup of the parent PID at callback time, marked `PT_FLAG_PARENT_OBSERVED`. It can refer to a reused PID and must not be represented as cryptographic proof of original ancestry. It is zero when unavailable.

For image events, `PT_FLAG_KERNEL_IMAGE` marks a kernel image (PID/key may be zero). `PT_FLAG_IDENTITY_UNKNOWN` marks an unavailable process identity. `PT_FLAG_TRUNCATED` marks a shortened name. Full paths are opt-in; otherwise only a basename is copied. UTF-16 truncation avoids ending on an unmatched high surrogate. A process exit normally has no image name or parent fields; use its process-start key to correlate it with an earlier create event.

QPC timestamps use `stats.frequency`. Relative milliseconds are `(event.qpc - stats.start_qpc) * 1000 / frequency`. Cross-thread callbacks can finish in a different order than their timestamp sampling; sequence numbers describe publication order. `start_utc_100ns` is a wall-clock anchor, not the monotonic timing source. This is notification observation timing only.

The 512-slot ring overwrites the oldest entry under overload. `received` counts published events, `dropped` counts overwritten events, and `truncated` counts shortened names. A consumer cannot reconstruct missing data. The desktop's 500-row retention and 64-batch handoff are separate, explicit limits; use the streaming CLI/SDK for longer observations. Neither the viewer nor CSV is tamper-proof.

CSV uses UTF-8, quotes image fields, escapes quotes and prefixes spreadsheet formula-leading image names. Export is local and explicit. Paths can disclose user/project names. Review reports before sharing them.
