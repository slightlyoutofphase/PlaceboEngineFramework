# Placebo Engine Framework (PEF)

*"The first fully instrumented Skyrim engine mod that provably does nothing."*

**Version 4.2.0.1b** — released last Tuesday. (The changelog tops out at 2.1. The version
number tops out at 4.2.0.1b. Neither is apologizing.)

PEF is a legitimate, professionally engineered CommonLibSSE-NG plugin — trampolines,
relocations, atomic counters, exception handlers, an audit thread — and its entire value
proposition is that it changes **zero bytes of game state, and can prove it**.

It is the honest mirror of every AI-generated "Crash Guard / Stability+ / FPS Optimizer"
DLL currently flooding the mod ecosystem: same architecture, same confident marketing,
zero claims about reality. The only thing left exposed is the difference between the
marketing and the machine.

---

## What it actually does (all technically real)

- **Hooks 47 hot engine call sites** via `SKSE::GetTrampoline().write_call<5>()` — the
  input poll that `Main::Update` performs every frame, the memory manager, the works.
  Every hook calls the original, passes every argument and the result through
  unmodified, and increments a counter called `frames_witnessed`.

- **Self-auditing worker thread**: every 250 ms it samples
  `std::atomic<std::uint64_t> operations_performed` and asserts it is still `0`. If the
  plugin is ever detected doing *something*, it writes an incident report and stops
  doing nothing immediately. This is called *Operational Integrity*.

- **Measured overhead**: `std::chrono::steady_clock` around each hook, aggregated into
  the log and the report — "this period, PEF did nothing in 0.4 µs per witnessed call.
  efficiently." The instrumentation cost is measured, reported, and owned. Nobody else
  ships their overhead; we ship ours, because ours is the product.

- **First responder alibi**: installs `AddVectoredExceptionHandler` at priority 1 and
  returns `EXCEPTION_CONTINUE_SEARCH`, so it executes *before* Crash Logger or
  Trainwreck on every crash. Every crash now contains, in `PEF_Alibi.txt`:

  ```
  PEF status at time of crash: dormant.
  Time spent altering game state: 0.000 ms.
  Suspects: everyone else.
  ```

  This is the Crash Guard parody distilled: instead of claiming to prevent crashes and
  blaming other DLLs, PEF formally establishes an alibi.

- **Shutdown report** written to `My Games/Skyrim Special Edition/PEF_Report.txt` — and
  refreshed every 10 seconds all session, so it is current even when the game dies
  without the courtesy of a shutdown:

  ```
  Frames witnessed:            1,204,551
  Allocations witnessed:       58,206,744 (pass-through, unmodified)
  Game state altered:                 0 times
  Median intervention time:           n/a
  Mean time between incidents:        ∞
  Uptime SLA:                         100% of crashes were caused by other plugins.
                                     Verified by us.
  ```

- **CommonLibSSE-NG flex**: the witness network is discovered by scanning the running
  image for real call sites, pinned by relocations that resolve on SE, AE, and VR.
  Porting to VR required zero changes, which we listed as a feature anyway.

## The proof-of-nothing pipeline

```
        engine call site            ┌──────────────────────────────────────────┐
   ┌────────────────────┐           │  audit thread (250 ms)                   │
   │  E8 CALL rel32     │           │                                          │
   │        │           │           │  assert operations_performed == 0        │
   │        ▼           │           │        │                                 │
   │  write_call<5>     │           │        ├─ still 0 ──► refresh report     │
   │        │           │           │        │              log status          │
   │        ▼           │           │        └─ nonzero ►  incident report     │
   │  witness thunk     │           │                     suspend witnessing   │
   │  ┌──────────────┐  │           │                     stop doing nothing   │
   │  │ call orig.   │  │           │                     immediately          │
   │  │ time it      │  │           └──────────────────────────────────────────┘
   │  │ count it      │ │
   │  │ alter nothing │ │
   └──┴──────────────┴──┘
```

## The Number 47

47 is a marketing constant with the full protection of engineering discipline. It lives
in `src/BuildInfo.hpp` as `hook_budget`, the installer treats it as a hard budget, and
it must never change — the README has already quoted it, and this paragraph is now load
bearing.

The engineering that fills the budget is honest about what it finds: call sites are
discovered by reading the code section of the running executable and locating real
five-byte `E8 rel32` CALL instructions whose targets are the engine's allocator and
input-poll functions (pinned by CommonLibSSE's own relocation tables — including VR
1.4.15). If the engine offers fewer hot sites than the budget, the report says so. The
marketing will not; that is what marketing is for.

## The configuration

Ships with a 60-line INI — `Data/SKSE/PlaceboEngineFramework.ini` — every key parsed,
validated, and range-checked:

```ini
[Memory]
; OptimizeAllocations — valid values: 0 or 1. Both do nothing.
; The setting exists so you feel involved.
OptimizeAllocations=0

[Threading]
WorkerThreads=8   ; Parsed, validated, then cherished. Not used.

[Culling]
AggressiveCulling=0  ; If 1, culls nothing slightly faster.
```

Unknown keys are not rejected; they are logged as *acknowledged* — by name, in the log,
so that you know that PEF knows that you tried. Three profiles ship in
`resources/profiles/` — **Performance**, **Balanced**, **Quality** — which are
byte-identical. This is called *marketing parity*, and it is enforced by the build
system: if any profile diverges, the build fails before a difference can ship.

The `[Instrumentation]` section is the one exception: the audit interval, the report
refresh period, and the status line cadence genuinely work. The product does nothing;
the instrumentation is how it proves that. Confusing the two is how other mods end up on
the Crash Guard pile.

## Requirements

- **SKSE** (Skyrim Script Extender)
- **Address Library** — all runtime variants
- **Null Core** — a separate 0 KB dependency mod containing only a README
  (`resources/null-core/` in this repository). PEF refuses to load without it; this
  feature is called *Dependency Integrity*
- One load order slot. The slot *is* the performance cost.

## Installation

1. Install SKSE, Address Library, and Null Core.
2. Copy `PlaceboEngineFramework.dll` to `Data/SKSE/Plugins/`.
3. Optionally copy `resources/PlaceboEngineFramework.ini` to `Data/SKSE/`.
4. Play. Nothing will happen. That is correct behavior, and it is monitored.

## FAQ

> **Q: My FPS didn't change.**
> A: Correct.

> **Q: Will this fix my crashes?**
> A: No, but it will be present when they happen — with documentation, an alibi, and a
> measured self-time. Honestly, that's more than most.

> **Q: Is it compatible with everything?**
> A: Everything, including itself. Incompatible with nothing except scrutiny.

> **Q: Where is the MCM?**
> A: The 2.1 toggle ("Enable PEF") lives in the INI. Toggling it off does nothing,
> because that is the product.

> **Q: Why does the report say the median intervention time is n/a?**
> A: The median of zero interventions is undefined. Reporting it as anything else would
> be the first actual lie in this file.

## Changelog

> **1.0** — Initial release.
> **1.1** — Fixed a crash in the audit thread. The plugin was doing something after
> all. Removed.
> **1.2** — Removed the fix from 1.1. It was also doing something.
> **2.0** — Rewritten from scratch to do nothing more efficiently.
> **2.1** — Added one toggle: "Enable PEF". Toggling it off does nothing, because that
> is the product.
> **4.2.0.1b** — Current. The number is seniority, not scope. Released last Tuesday.

## Building from source

Standard CMake + MSVC. **No vcpkg** — the dependency surface (spdlog, DirectXTK,
rapidcsv) is fetched by CMake itself via `cmake/Dependencies.cmake` and satisfied
through `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`, which is the documented mechanism for
feeding a `find_package()` tree from in-tree FetchContent targets.

Requirements: Visual Studio 2022 (MSVC v143), CMake 3.25+, git, and network access at
configure time (first configure only; everything is cached afterwards).

```bash
git clone --recursive <this repository> pef
cd pef

cmake --preset windows-msvc            # or: windows-ninja, from a VS developer prompt
cmake --build --preset windows-msvc-release
```

The DLL lands in `build/windows-msvc/Release/PlaceboEngineFramework.dll`.

Notes for the impatient:

- CommonLibSSE-NG is consumed **as a git submodule** (`extern/CommonLibSSE-NG`, branch
  `ng`). `git submodule update --init --recursive` is mandatory — the recursive step
  fetches its `openvr` submodule.
- Compiling CommonLibSSE from source takes a while. For a fast first build you may set
  `COMMONLIB_PREBUILT=1` (when the submodule is pinned to a clean release tag) to link
  the published prebuilt bundle instead — no vcpkg required either way. See
  `extern/CommonLibSSE-NG/PREBUILT.md`.
- The `marketing_parity` target runs on every build and fails the build if the three
  profiles ever diverge. Product requirements belong in the build system, not in a
  wiki page someone will forget to update.

### Project layout

```
├── CMakeLists.txt              plugin target via add_commonlibsse_plugin()
├── CMakePresets.json           windows-msvc / windows-ninja
├── cmake/Dependencies.cmake    spdlog + DirectXTK + rapidcsv, fetched, no vcpkg
├── extern/CommonLibSSE-NG/     submodule (branch ng)
├── resources/                  the INI, three identical profiles, Null Core
└── src/
    ├── Main.cpp                the load entry point — the only exported function
    ├── Plugin.cpp              lifetime orchestration
    ├── BuildInfo.hpp           identity constants, including the budget (47)
    ├── Audit/AuditThread.*     Operational Integrity loop (std::jthread)
    ├── Config/Settings.*       INI parsing, validation, acknowledgment
    ├── Crash/AlibiHandler.*    priority-1 vectored exception handler
    ├── Diagnostics/            logging facade, atomic telemetry
    ├── Hooks/                  scanner, witness slots, installer
    └── Report/ReportWriter.*   PEF_Report.txt / PEF_Incident.txt
```

### Engineering notes (the part other pages put under "Performance")

- **Memory ordering, pedantically applied.** Statistics counters use relaxed ordering —
  they are numbers, not synchronization. The `operations_performed` counter uses
  release on the (never-taken) write path and acquire on every audit read, so that when
  the impossible happens, the incident report can name the exact line that broke the
  product's one promise.
- **False sharing is accounted for.** Every witness slot's statics are padded to a cache
  line (`std::hardware_destructive_interference_size`), because inflating the overhead
  we are contractually obligated to report honestly would be self-defeating even by
  this project's standards.
- **The crash handler is written to a crashing-thread discipline.** No locks, no heap,
  no spdlog — its sinks take mutexes, and a mutex on a dying thread is how one crash
  becomes two. The alibi is rendered into a stack buffer and written with one
  `CreateFileW` / `WriteFile` / `CloseHandle`.
- **Every hook is a five-byte call-site patch over a real E8 CALL.** No entry-point
  detours, no instruction splitting: the instruction being replaced is exactly the
  instruction the trampoline is designed to replace. Call sites are found by scanning
  the running image, not by trusting a comment section.
- **The plugin version lives in CMake.** `add_commonlibsse_plugin()` generates the
  `SKSEPluginInfo` declaration from the build system, so the binary's version cannot
  drift from the repository's. There is no second place to forget to update.

## Credits

- **[CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)** — alandtse and
  contributors: the cross-runtime (SE/AE/VR) engine library this plugin does nothing on
  top of.
- **SKSE team** — the script extender, the trampoline, the address library ecosystem.
- Everyone who shipped a "Crash Guard" DLL with a changelog full of fixes for crashes
  the previous version introduced. You were the specification.

## License

MIT — see `LICENSE`. The satire is free; the engineering is real; the two are not in
conflict, and that was the entire point.

---

*PEF — provably nothing, since last Tuesday.*
