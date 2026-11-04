# Placebo Engine Framework (PEF)

**Engine instrumentation, operational integrity, and documented non-intervention for Skyrim SE, AE, and VR.**

*"If nothing changes, everything is working as intended."*

**Current version:** `4.2.0.1b`  
**Operational status:** Dormant by design.

Placebo Engine Framework is an SKSE plugin built with CommonLibSSE-NG. It installs pass-through witnesses at selected engine call sites, records what those witnesses observe, and continuously audits its own commitment to making no gameplay-state interventions.

PEF does not improve frame rates, optimize memory allocation, prevent crashes, or correct engine behavior. Those activities would introduce changes to the system under observation, placing them outside the framework's operational mandate.

Instead, PEF delivers something more narrowly defined: **a running, timestamped account of how consistently it has refrained from helping.**

The distinction is important. Simply doing nothing requires no software. *Verifying* that nothing has been done requires an architecture.

## Features

### Engine Witness Network

PEF can instrument **up to 47 engine call sites** using SKSE's trampoline interface and five-byte `CALL` replacements. The witness installer searches the executable's code section for calls to relocation-resolved engine functions, including memory allocation, reallocation, deallocation, and input polling.

Each witness forwards the original arguments to the original function and returns its result unchanged. It also records invocation counts and elapsed time. PEF considers observation a responsibility, not permission to intervene.

The number **47** is a compile-time capacity limit, not a promise that every runtime will offer 47 eligible locations. Actual installations are reported without rounding up. The limit itself is non-negotiable.

### Operational Integrity

An independent audit thread samples PEF's internal `operations_performed` counter every **250 ms** by default. The accepted value is **zero**.

If the counter becomes nonzero, PEF records an Operational Integrity incident in `PEF_Incident.txt` and suspends active witness instrumentation for the remainder of the session. Installed hooks continue forwarding calls to their original targets.

This safeguard is intentionally uncompromising: if an intervention is registered, witness instrumentation is suspended for the rest of the session. Monitoring the suspension, naturally, continues.

The audit verifies PEF's own registered operations. It is not an independent scan of all Skyrim memory, and it makes no claims about changes introduced elsewhere in the process. PEF's standards are strict, but its jurisdiction is limited.

### Incident Alibi System

PEF registers a first-priority Windows vectored exception handler using `AddVectoredExceptionHandler(1, ...)`. For selected exception codes associated with serious failures, it attempts to write `PEF_Alibi.txt` before returning `EXCEPTION_CONTINUE_SEARCH` so normal exception processing can continue.

A typical alibi begins:

```text
PEF status at time of crash: dormant.
Time spent altering game state: 0.000 ms.
Suspects: everyone else.
```

The filing also includes the exception code, alibi sequence number, and number of frames witnessed. Alibi creation is rate-limited to avoid redundant filings during a sequence of exceptions.

**PEF does not suppress, repair, or diagnose the exception.** The alibi is a record of the framework's declared state, not a determination of fault. Responsibility for a crash remains with the appropriate investigative authorities. PEF has already submitted its statement.

### Telemetry and Session Reports

PEF tracks witnessed frame-poll calls, memory-management calls, exceptions, alibi filings, and the elapsed duration of witnessed calls. These counters are summarized in `PEF_Report.txt`, refreshed **every 10 seconds** by default and finalized during normal shutdown.

An illustrative excerpt:

```text
Frames witnessed:                 1,204,551
Allocations witnessed:            58,206,744 (pass-through, unmodified)
Game state altered:               0 times
Median intervention time:         n/a (there were no interventions)
Mean time between incidents:      ∞
Uptime SLA:                       100% of crashes were caused by other plugins.
                                  Verified by us.
```

The report's `Game state altered` field reflects PEF's internal operation counter. The more expansive assertions in its service-level declaration are provided by PEF's own reporting department, which has not requested outside review.

Witness timings include the original engine call as well as PEF's instrumentation; they should not be interpreted as an isolated measurement of plugin overhead. The time was measured correctly. Whether the time was well spent is outside the scope of telemetry.

### Runtime Coverage

PEF uses CommonLibSSE-NG relocation identifiers and runtime-aware address resolution to target Skyrim Special Edition, Anniversary Edition, and Skyrim VR. Witness coverage is determined at runtime and may vary with the executable.

Unsupported or undiscovered call sites are not replaced. A smaller witness network is still a valid witness network. PEF declines to invent coverage merely to meet a target.

## How it works

```text
    Skyrim engine call site
               |
               v
       Trampoline witness
               |
       +-------+--------+
       |                |
       v                v
  Call original    Record telemetry
  unmodified       (count + time)
       |                |
       v                v
  Return result      Audit thread
  unmodified         every 250 ms
                         |
                 operations_performed?
                    /          \
                   0          nonzero
                   |             |
             Refresh status  Write incident
             and continue    Suspend witnessing
```

The design separates three responsibilities:

1. **Witnessing:** observe selected engine calls and preserve their behavior.
2. **Auditing:** confirm the internal intervention counter remains at its required value.
3. **Reporting:** retain an account of both, whether anyone requested one or not.

PEF uses real trampoline patches, atomic counters, a dedicated audit thread, and Windows exception handling. The absence of game improvements is a design objective, not a shortcut in implementation.

## Configuration

The configuration file is located at `Data/SKSE/PlaceboEngineFramework.ini`.

```ini
[Memory]
OptimizeAllocations=0

[Threading]
WorkerThreads=8

[Culling]
AggressiveCulling=0

[Product]
EnablePEF=1
Profile=Balanced

[Instrumentation]
AuditIntervalMs=250
ReportRefreshSeconds=10
StatusLogSeconds=30
```

Every recognized setting is parsed and validated. What happens after validation depends on the setting's responsibilities.

| Setting | Accepted values | Effect |
| --- | --- | --- |
| `OptimizeAllocations` | `0` or `1` | Records the selected allocation-optimization policy. No allocator behavior is changed. |
| `WorkerThreads` | `1`–`64` | Retains the requested worker-thread allocation. No additional workers are created for gameplay optimization. |
| `AggressiveCulling` | `0` or `1` | Records the selected culling policy. No objects are culled by PEF. |
| `EnablePEF` | `0` or `1` | Records the requested product state. Neither value changes runtime behavior. |
| `Profile` | `Performance`, `Balanced`, `Quality` | Selects the named configuration profile. All three have identical contents. |
| `AuditIntervalMs` | `50`–`60000` | Sets the operational audit interval in milliseconds. |
| `ReportRefreshSeconds` | `1`–`600` | Sets the status-report refresh interval in seconds. |
| `StatusLogSeconds` | `1`–`3600` | Sets the periodic log interval in seconds. |

Unknown keys are not rejected. They are **acknowledged** in the log. This confirms that the requested setting was received, even when PEF has no particular plans for it.

### Performance, Balanced, and Quality

PEF ships with three profiles in `resources/profiles/`:

- **Performance** — for users who prioritize performance.
- **Balanced** — for users who prefer a balanced configuration.
- **Quality** — for users who prioritize quality.

The profile files are byte-identical. This is an enforced compatibility requirement: the `marketing_parity` build target fails if any profile differs from the others.

Choosing a profile is therefore safe. No unexpected changes in performance, quality, or behavior can result from the selection. Users are encouraged to choose carefully nonetheless.

The `[Instrumentation]` settings are different: they genuinely control auditing and reporting. They do not change the engine, but they do change how often PEF documents its decision not to.

## Requirements

- **Skyrim SE, AE, or VR**, with a compatible SKSE installation.
- **Address Library** appropriate for the selected runtime.
- **Null Core**, the mandatory resource-only dependency supplied in `resources/null-core/`.

### Null Core and Dependency Integrity

Null Core consists of a README and no executable code, scripts, or runtime assets. Its operational footprint is zero. Its administrative importance is considerably greater.

At startup, PEF checks for a Null Core manifest at one of the following locations:

```text
Data/NullCore/README.md
Data/NullCore/README.txt
```

If neither file exists, PEF refuses to initialize. This check is known as **Dependency Integrity**.

Null Core does not need to perform any work to satisfy the dependency requirement. Presence is sufficient; activity would be a scope expansion.

## Installation

1. Install the appropriate SKSE and Address Library for your game runtime.
2. Install Null Core so that `Data/NullCore/README.md` exists in the game directory.
3. Place `PlaceboEngineFramework.dll` in `Data/SKSE/Plugins/`.
4. Optionally place `resources/PlaceboEngineFramework.ini` at `Data/SKSE/PlaceboEngineFramework.ini`.
5. Launch the game through SKSE.

Successful installation should not produce a visible gameplay change. Check the SKSE log and `PEF_Report.txt` for evidence that PEF has initialized and is maintaining its non-intervention policy.

Removing the DLL also produces no intended gameplay change. It does, however, terminate PEF's ability to document that outcome, which is not the same thing.

## Reports and diagnostics

PEF writes its reports to the game's user-data directory, normally under `Documents/My Games/<game>/`. If SKSE's log-directory location is unavailable, the plugin falls back to the game directory.

| File | Purpose |
| --- | --- |
| `PEF_Report.txt` | Periodic telemetry, witness coverage, configuration, and final session status. |
| `PEF_Alibi.txt` | Best-effort non-intervention statement for selected exceptions. |
| `PEF_Incident.txt` | Operational Integrity failure record, created if an intervention is registered. |

`PEF_Report.txt` is refreshed during play rather than only at shutdown. This provides a recent account of the framework's activities even when a session ends without requesting a final report.

The absence of an alibi does not establish the absence of a crash. The presence of an alibi does not establish the cause. PEF's documentation is extensive, but it is not clairvoyant.

## FAQ

**Will PEF increase my FPS?**

No. There is no frame-rate improvement mechanism. If your FPS is unchanged, the relevant acceptance criterion has been met.

**Will it fix crashes?**

No. It can attempt to document selected exception events, but exception recovery is not implemented. Preventing a crash would compromise the incident-reporting workflow.

**Does PEF really do nothing?**

It does quite a lot of work to avoid making gameplay-state interventions. It installs hooks, maintains counters, runs an audit thread, and writes reports. None of those activities is intended to optimize or repair Skyrim. PEF is very busy within its carefully limited remit.

**Why is the witness count lower than 47?**

Forty-seven is the maximum registration budget. The actual count depends on suitable call sites found in the active runtime. Missing opportunities are reported as missing, not quietly filled with imaginary witnesses.

**Does `EnablePEF=0` disable the plugin?**

No. The setting is recognized and recorded, but does not currently alter runtime behavior. Both values are supported with complete functional parity.

**Which configuration profile should I use?**

All three are supported. Performance, Balanced, and Quality are byte-identical, so the decision is primarily a matter of preference. PEF cannot make that decision for you.

**Why is median intervention time listed as `n/a`?**

The median of zero observed interventions is undefined. Reporting a numerical value would require an intervention, which would then invalidate the premise of the measurement.

**Is PEF compatible with every other plugin?**

PEF does not intentionally change engine behavior, but it does patch executable call sites and register an exception handler. Compatibility therefore cannot be guaranteed universally. It documents its own activity rather than making promises on behalf of unrelated software.

**Where is the MCM?**

Configuration is provided through the INI file. A separate in-game menu is not included. Adding one would substantially increase the number of things PEF does.

## Building from source

PEF uses **CMake, MSVC, C++23, and CommonLibSSE-NG**. Dependencies are resolved through the repository's CMake configuration; vcpkg is not required.

### Prerequisites

- Windows x64.
- Visual Studio 2022 or 2026 with the C++ build tools (or an equivalent MSVC developer environment).
- CMake 3.25 or newer for the supported presets. The native Visual Studio 2026 preset requires CMake 4.2 or newer.
- Git, including submodule support.
- Network access during initial dependency acquisition.

### Default build

```powershell
git clone --recursive https://github.com/slightlyoutofphase/PlaceboEngineFramework.git
cd PlaceboEngineFramework

cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
```

The default preset uses **Ninja Multi-Config** with MSVC. The expected Release output location is:

```text
build/windows-msvc-ninja-multi/Release/PlaceboEngineFramework.dll
```

Additional presets are available in `CMakePresets.json` for native Visual Studio 2022/2026 projects and single-configuration Ninja builds.

CommonLibSSE-NG is a Git submodule at `extern/CommonLibSSE-NG/`. If the repository was cloned without `--recursive`, initialize it with:

```powershell
git submodule update --init --recursive
```

The recursive step is necessary for CommonLibSSE-NG's nested dependencies, including OpenVR. The `marketing_parity` target runs as part of the default build to verify that all three configuration profiles remain identical. Release engineering takes this requirement seriously.

### Project structure

```text
├── CMakeLists.txt                 Build configuration and plugin target
├── CMakePresets.json              MSVC and Ninja build presets
├── cmake/Dependencies.cmake       Third-party dependency resolution
├── extern/CommonLibSSE-NG/        Cross-runtime engine library (submodule)
├── resources/
│   ├── PlaceboEngineFramework.ini Default configuration
│   ├── profiles/                  Three equivalent operating profiles
│   └── null-core/                 Mandatory dependency manifest
└── src/
    ├── Main.cpp                   SKSE entry point
    ├── Plugin.cpp                 Initialization and lifecycle
    ├── BuildInfo.hpp              Version and instrumentation budget
    ├── Audit/                     Operational Integrity thread
    ├── Config/                    INI parsing and validation
    ├── Crash/                     Exception monitoring and alibi filing
    ├── Diagnostics/               Logging and telemetry
    ├── Hooks/                     Call-site discovery and witness installation
    └── Report/                    Status and incident reporting
```

### Engineering notes

- **Pass-through hooks.** Witness thunks call the original engine functions with the original arguments and propagate their results. They do not contain alternate gameplay implementations.
- **Atomic telemetry.** High-frequency counters use relaxed atomic operations; the intervention counter uses release/acquire ordering to support the audit mechanism.
- **False-sharing mitigation.** Witness state is aligned using `std::hardware_destructive_interference_size`. Monitoring nothing should not create unnecessary contention over it.
- **Exception handling.** The alibi path avoids ordinary logging sinks and attempts a direct file write using Windows APIs. The handler always returns `EXCEPTION_CONTINUE_SEARCH`.
- **Call-site discovery.** The installer scans the executable's code section for matching `E8 rel32` call targets and uses the SKSE trampoline to install witnesses.
- **Versioning.** CMake provides the plugin's build version; the source maintains its report-facing version and channel designation. The `b` channel remains under active interpretation.

## Changelog

**4.2.0.1b** — Current release. Operational scope unchanged. Release classification expanded.

**2.1** — Added the `EnablePEF` setting. Both supported values provide equivalent behavior.

**2.0** — Rebuilt the framework around a revised non-intervention architecture. No change to intended gameplay outcomes.

**1.2** — Reverted the previous audit-thread correction following additional review of its necessity.

**1.1** — Corrected an audit-thread crash. The correction itself was subsequently reviewed for unauthorized functionality.

**1.0** — Initial release. Established the baseline against which all subsequent non-interventions are measured.

## Credits

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) — alandtse and contributors.
- The SKSE team and contributors to the Address Library ecosystem.

## License

MIT. See [LICENSE](LICENSE).

---

*Placebo Engine Framework — committed to leaving well enough alone, whether or not it was well enough to begin with.*
