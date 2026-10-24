# Validation status

Version 0.1.0, Windows x64 engineering preview. The new kernel package is **unsigned and awaiting Microsoft submission**. No host or VM driver installation, live process capture, Driver Verifier campaign or HLK certification is claimed.

Completed locally:

- Original KMDF driver: WDK 10.0.26100.0, KMDF 1.31, MSVC `/W4 /WX`, PREfast, CFG, `/INTEGRITYCHECK`; build succeeds without compiler/analyzer warnings.
- INF: `InfVerif /w`, `InfVerif /u`, and `Inf2Cat` for the declared Windows 10/11 x64 targets succeed. This validates package format, not runtime behavior.
- GUI, CLI, SDK, installer sources and tests compile with MSVC 14.44.35207 and C++17, including the full application/SDK under AddressSanitizer.
- Three offline CTest suites pass in Release and ASAN: wire/ring/privacy/identity validation, synthetic CLI CSV preview, and installer package-identity/signature rejection.
- The protocol suite performs 579,766 assertions, including 100,000 randomized ring operations against an independent deque, overflow ordering, exact requests/reserved flags, name/path privacy, UTF-16 truncation, reused-PID identities, closed/stale-owner/removal Start rejection and 20,000 malformed batches. These exercise shared primitives, not the Windows kernel's concurrency implementation.
- The CAB is expanded again and its INF/SYS/CAT/PDB hashes compared with their sources. Its submission folder is `TitanProcessTrace` (17 characters).
- `docs/app.png` is a preview of synthetic events. No real process list or private machine data is displayed.

Required after signing, before calling the driver production-ready:

1. Install/uninstall/reinstall in disposable Windows 10 and 11 x64 VMs; verify signature, device identity, elevation, error reporting and restart handling.
2. Generate short-lived child processes and DLL/image loads under a controlled harness. Compare create/exit identities and statuses with harness evidence; verify PID reuse and missing-parent semantics.
3. Stress Start/Stop, timeout/cancellation, process termination, simultaneous clients, handle closure while Start/Receive is submitted, ring overflow and slow consumers. Confirm capture cannot reactivate after cleanup and memory remains bounded.
4. Exercise removal/uninstall/restart while a reader is pending. Run Driver Verifier with an attached debugger, HVCI/Memory Integrity, Secure Boot and available Windows servicing variants.
5. Measure callback overhead, event throughput and idle usage on stated hardware. No zero-overhead, lossless, zero-latency or universal-compatibility claim has been made.

The installer is designed to reject an unsigned or unrelated catalog/package, serialize setup, preserve unrelated devices/packages, report Windows failures and expose a restart button only when requested. Its offline identity/trust tests do not prove live recovery behavior. Do not publish an unsigned installer as a working signed release.
