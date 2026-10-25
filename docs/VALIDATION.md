# Validation status

## Returned Microsoft package (2026-10-25)

The Microsoft Hardware Compatibility Publisher catalog, SYS/INF membership and
preserved submitted executable content were checked. The self-contained console
installer then installed the exact returned package on Windows 10 build 19045
x64; existing real-driver functional tests passed, and the owned node was removed
and the driver unloaded. The VM's original baseline and nine historical snapshots
were preserved. This campaign used the Microsoft-signed SYS/CAT without adding a
guest test signature. The guest was in test mode for a separate unsigned network
project, so Secure Boot/HVCI compatibility is not established by this result.
The returned package also passed the repeated real-driver campaign with targeted
standard Driver Verifier enabled. The UI install/uninstall/recovery paths and
Windows 11 still need broader testing.

Version 0.1.0, Windows x64 engineering preview. The submission kernel package is **the original unsigned input, now returned by Microsoft with a trusted signature**. A copy was test-signed with a temporary certificate inside a disposable Windows 10 VM for the real checks below. That guest test signature is not a Microsoft signature. No driver was installed on the host, and no host security settings were changed.

Completed locally:

- Original KMDF driver: WDK 10.0.26100.0, KMDF 1.31, MSVC `/W4 /WX`, PREfast, CFG, `/INTEGRITYCHECK`; build succeeds without compiler/analyzer warnings.
- INF: `InfVerif /w`, `InfVerif /u`, and `Inf2Cat` for the declared Windows 10/11 x64 targets succeed. This validates package format, not runtime behavior.
- GUI, CLI, SDK, installer sources and tests compile with MSVC 14.44.35207 and C++17, including the full application/SDK under AddressSanitizer.
- Three offline CTest suites pass in Release and ASAN: wire/ring/privacy/identity validation, synthetic CLI CSV preview, and installer package-identity/signature rejection.
- The protocol suite performs 579,766 assertions, including 100,000 randomized ring operations against an independent deque, overflow ordering, exact requests/reserved flags, name/path privacy, UTF-16 truncation, reused-PID identities, closed/stale-owner/removal Start rejection and 20,000 malformed batches. These exercise shared primitives, not the Windows kernel's concurrency implementation.
- The CAB is expanded again and its INF/SYS/CAT/PDB hashes compared with their sources. Its submission folder is `TitanProcessTrace` (17 characters).
- `docs/app.png` is a preview of synthetic events. No real process list or private machine data is displayed.

## Real Windows 10 guest checks

On Windows 10 `10.0.19045`, an initial controlled test-signed campaign completed **2,516 live assertions**:

- A real, owned short-lived child process produced create and exit events. Its creation time matched `GetProcessTimes`, its process-start key matched across create/exit, its live parent observation matched the harness, and its exit status was the expected `17`.
- A second capture owner was refused.
- 100 Start/Stop/close cycles checked session advancement, inactive Stop state and automatic cleanup on client destruction.
- 100 overlapped Start/close races and 100 pending Receive/close races completed. Reopening the device after each operation confirmed capture remained inactive.
- The driver unloaded and its owned root device node was removed. The VM returned to its original Off baseline; its nine original checkpoints were preserved.

Driver Verifier was disabled during that initial campaign.

A second **short, targeted Driver Verifier campaign** passed **2,546 live assertions** on the same Windows 10 version. Verifier enable/query evidence identifies exactly `TitanProcessTrace.sys` and the companion `TitanConnectionTrace.sys`, using flags `0x001209bb` (including Special Pool, forced IRQL, pool tracking, I/O, deadlock, security and DDI checks). This was not an exhaustive or long-duration verifier/fault-injection campaign.

The second campaign repeated process create/exit identity, expected exit `17`, ownership, 100 capture/close cycles and 200 raw close-race operations. An additional owned `cmd.exe` child with exit `23` produced **four non-empty IMAGE basenames**. Full-path exposure was rejected and known image identities matched its CREATE identity. The report recorded **zero unknown identities and zero missing names** in that observation. The driver unloaded and its root node was removed; the guest returned to its original Off baseline with the original checkpoints intact.

Both campaigns used guest-only test signing with Secure Boot off. HVCI/Memory Integrity was not validated. Actual operating-system PID reuse was not forced; reused PID values are covered only by offline identity tests. No arbitrary kernel image was loaded, so kernel-image-load coverage is not claimed. Windows 11, the self-contained installers, long-duration/failure stress and production performance benchmarks still require separate coverage.

Private evidence binds the exact unsigned source-package SYS hash to the separately test-signed guest copy. Adding the test signature changes the installed SYS hash; the two hashes are deliberately not represented as identical. The submission CAB itself remains unsigned and unchanged by the guest test. Kernel source, private reports and guest signing material are not included in the public repository.

Required after signing, before calling the driver production-ready:

1. Validate the Microsoft-signed package and self-contained UI/console install/uninstall/reinstall/recovery in disposable Windows 10 and 11 x64 VMs; verify device identity, elevation, errors and restart handling. The earlier guest campaign used `devcon`, not the public installer UI.
2. Extend the owned user-process image observations to broader DLL/kernel-image cases, actual PID reuse where reproducible and unavailable-parent cases.
3. Broaden concurrent capture, process termination, overflow and slow-consumer stress. The current close/start/receive races are useful coverage, not proof that every Windows concurrency case is covered.
4. Extend targeted Verifier coverage with longer runs and controlled failure scenarios, removal/uninstall/restart while readers are pending, an attached debugger, HVCI/Memory Integrity, Secure Boot and additional Windows servicing variants.
5. Measure callback overhead, event throughput and idle usage on stated hardware. No zero-overhead, lossless, zero-latency or universal-compatibility claim has been made.

The installer is designed to reject an unsigned or unrelated catalog/package, serialize setup, preserve unrelated devices/packages, report Windows failures and expose a restart button only when requested. Its offline identity/trust tests do not prove live recovery behavior. Do not publish an unsigned installer as a working signed release.
