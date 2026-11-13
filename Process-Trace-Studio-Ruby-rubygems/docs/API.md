# Process-Trace-Studio operation schema

The unchanged product C++ SDK is called directly. Operations: `check`, `start`, `stop`, `cancel`, `status`, `receive`, `format_batch`, `event_name`. The dispatch source in `Bridge.cpp` lists exact argument keys and maps every operation to the original public SDK. Protocol structs retain their public-header field names, including case. UInt64 and Int64 fields are decimal strings; fixed arrays accept at most their capacity and omitted fields initialize to zero. Unknown fields and out-of-range values are rejected. SDK validators apply original protocol constraints.

Wire types and fields:

- `PT_REQUEST`: version, size, flags, reserved
- `PT_EVENT`: sequence, qpc, process_key, create_time_100ns, parent_key, type, pid, parent_pid, exit_status, flags, name_chars, name
- `PT_STATS`: version, size, active, options, session, frequency, start_qpc, start_utc_100ns, received, dropped, truncated, queued, capacity, reserved
- `PT_BATCH`: stats, count, reserved, events

Only explicit start/connect/policy calls start the corresponding operation. The C ABI session constructor is inert; one active session per product DLL. Always close. Capture receives are bounded when the native API accepts a timeout. SDK original licensing/trial checks remain unchanged. No driver is installed by packages.

Tests exercise synthetic wire inputs using the real production SDK validation/formatting helpers, rejected arguments, exact unsigned values and inert session lifecycle. No live capture, policy application, storage fault, traffic modification or driver activation is performed by these tests. Operational driver paths require a separate integration environment.

## Language client

The client calls the native SDK directly. Use the call method with an operation name and argument object. Calls are synchronous; only operations exposing a receive timeout are capped at100ms; use an application worker for responsive UI/event loops. Explicitly close the client after use. 64-bit identifiers and counters are decimal strings. Native errors expose their Windows code. Matching driver and licensing are separate requirements.
