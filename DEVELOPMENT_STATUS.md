# Taskbar Monitor development status

## Phase 6 · Release 1.0.0

A fresh Visual Studio 2022 x64 CMake build produced both editions with the static MSVC runtime. The display, monitoring, placement and foundation suites passed in Debug (4/4) and Release (4/4). Both PE files have x64 machine type, icon, version and settings resources. Direct imports are Windows DLLs for graphics, shell, registry, system metrics and networking. Both delivered applications launched on the development desktop.

| Windows 11 development-machine check | Observed result |
| --- | --- |
| Clean installation with existing per-user settings | Program Files executable matched the Release build by SHA-256. Both shortcut targets and the quoted current-user Run command matched its installed path. The existing configuration hash remained identical. |
| Installed first launch and persistence | Settings loaded the existing font. The overlay saved an anchor change on exit and restored its original value on a second launch. The full configuration returned to its original hash. |
| Portable extraction and isolation | The extracted program ran beside the installed process. Its disabled startup checkbox, independent dialog, 16-DIP font save and relaunch were verified. Installed configuration and startup registration were unchanged by the portable operations. |
| Running process | Silent Setup and Uninstall each returned exit 1 while the installed application retained its instance mutex. Both proceeded after exit. |
| Upgrade | A test-only 0.9.9 installer registration containing the same application payload upgraded to 1.0.0. The installed executable matched the release, the Run entry survived and configuration bytes were preserved. There was no earlier public installer. |
| Uninstall and reinstall | Silent uninstall removed installed files, shortcuts, Apps registration and the matching startup entry. Reinstall with both optional tasks off passed. Final uninstall restored the pre-test machine state for this product, including the original user configuration. |

The two editions and installer are unsigned. No clean VM was available for this run. The optional affirmative deletion of the user's real settings file, the interactive finish-page launch checkbox, alternate administrator credentials and a real sign-out/sign-in were not exercised. Neither an installed developer toolchain nor this machine's successful launches establish clean-machine compatibility. A reinstall invocation returned exit 2 before producing a Setup log. The subsequent explicit task-deselection invocation passed.

## Phase 5 · Hardening and compatibility

MSVC x64 Debug and Release each passed all four CTest suites. The first sandboxed Debug run failed the localhost ICMP and temporary HKCU registration checks. Both passed with ordinary Windows desktop permissions. New regressions cover incremental DirectWrite layout creation, signed and overflowing PDH fields, maximum-width LUIDs, and JSON output on either side of the 1 MiB loader limit.

| Windows 11 Release run | CPU, one core | Working set, MiB | Private bytes, MiB | Paints / text changes | Scheduler bookkeeping |
| --- | ---: | ---: | ---: | ---: | ---: |
| Before changes, 60.02 s | 0.2864% | 24.28 → 24.28 | 9.86 → 9.74 | 50 / 50 | 176 µs |
| After changes, 60.09 s | 0.3900% | 24.08 → 24.33 | 9.18 → 9.38 | 59 / 59 | 163 µs |
| Explicit primary monitor, 300.34 s | 0.3642% | 24.31 → 20.09 | 9.76 → 9.60 | 146 / 146 | 984 µs |
| Settings open, 60.16 s | 0.1558% | 26.83 → 26.95 | 10.10 → 10.02 | 60 / 60 | 255 µs |
| UI-thread profile, 90.15 s | 0.5547% | 24.15 → 24.29 | 9.50 → 9.41 | 89 / 89 | 469 µs |

The UI thread accounted for 156.25 ms of the last run's 500.00 ms total CPU (31%). This includes geometry, text layout, painting and other message handling. Other threads used the remaining 343.75 ms. Timing varied with desktop activity across samples.

The five-minute sample took readings every ten seconds. Handles ranged from 249 to 257 and ended at 249. Threads ranged from 6 to 12 and ended at 6. Private bytes peaked at 9.79 MiB. The process stayed responsive and exited cleanly. The samples show no sustained growth over that run. They do not establish 24-hour stability.

One resident-page census found 16.84 MiB currently shared with other processes out of 24.17 MiB resident. Private allocations accounted for 6.16 MiB resident. Mapped modules included 1.23 MiB Direct2D, 1.05 MiB D3D11, 0.94 MiB WARP and 0.91 MiB DirectWrite. Resident shared pages alone exceeded the original 15 MiB working-set budget in this sample.

Separate warmed, single-collector processes ran concurrently for one minute at default intervals. Network collection used 78.12 ms of process CPU and averaged 1,699 µs elapsed per call. GPU collection averaged 846 µs across 24 engines. The other processes reported zero incremental CPU at Windows timer resolution. These isolated measurements identify network enumeration as a contributor and do not fully attribute total overlay CPU. No sustained whole-process CPU improvement or compliance with the 0.2% target was demonstrated.

Live checks on the single 1920 × 1200, 144-DPI display passed explicit primary selection, missing-monitor hiding and close, both overlay anchor positions, focus, duplicate launch, auto-hide retract/reveal, Settings open/close and normal shutdown. Targeted display-change and TaskbarCreated notifications preserved placement. The placement suite covered synthetic secondary-monitor coordinates and 100%, 125%, 150% and 200% scales. The monitoring suite exercised adapter removal, reconnection and counter resets. During a restricted 20-snapshot run, the GPU counter stayed unavailable and ping reported error while network, CPU and RAM continued updating.

The all-collector Release diagnostic completed 600 one-second snapshots and exited successfully. GPU utilization was numeric in 599 snapshots. All 600 reported unsupported CPU temperature and unavailable GPU temperature. Mean collector wall times in µs were network 2,746, CPU 191, RAM 6, GPU 1,884, temperature 50 and ping 257. Scheduler bookkeeping totaled 5,535 µs across 1,619 reported wakes.

Physical secondary monitors, mixed-DPI transitions, actual resolution or taskbar-button alignment changes, physical network reconnection, and live GPU driver removal/recovery remain unverified. The user requested non-disruptive checks on this session. Previous phases verified actual Explorer replacement. Phase 5 did not restart Explorer or sign out. The startup helper passed its isolated current-user registry test.

## Phase 4 · Settings and customization

Final MSVC x64 builds passed all four CTest suites in both Debug and Release. Desktop checks passed focus preservation, duplicate launch, auto-hide and clean shutdown. With all eight metrics disabled, a second launch with --settings opened the dialog in the hidden original instance. The centered native dialog and customized overlay were visually inspected at 150% scaling.

| Release, 30 seconds per run | CPU time / one-core average | Working set | Private bytes | Overlay paints / text changes | Scheduler bookkeeping |
| --- | --- | --- | --- | --- | ---: |
| Settings closed after Save | 171.88 ms / 0.5725% | 30.07 → 30.12 MiB | 9.90 → 9.88 MiB | 30 / 30 | 194 µs |
| Settings open | 109.38 ms / 0.3645% | 26.80 → 26.80 MiB | 9.70 → 9.66 MiB | 30 / 30 | 124 µs |

These were separate application launches with the same saved six-metric configuration. The closed run had already exercised font validation, Save and graphics resource recreation. Scheduler bookkeeping excludes collector execution and waits. Neither sample met the initial CPU or working-set targets. Both tests restored the user's prior configuration.

The native settings dialog opens from the live overlay's context menu or the --settings launch option. Save publishes the validated draft in the same application process. The editor supports visibility and ordering of eight metrics, system/light/dark/custom overlay palettes, RGB colors, font family and size, spacing, padding, compact layout, six collection intervals, startup registration and placement on an enumerated monitor. Left/right anchoring and vertical offsets apply inside horizontal taskbars.

The live desktop test used a 1920 × 1200 Windows 11 display at 144 DPI. It cancelled a Restore Defaults draft and compared the original configuration, submitted an invalid font size, and then successfully saved a 14-DIP Consolas font, custom colors, upload-first ordering and an 800 ms network interval. The captured overlay showed the updated order and appearance. The test restored the original configuration after the run.

Debug and Release CTest coverage includes the complete settings round trip, field and order repair, rejection of an invalid save, failed file replacement, palette selection, reordered and hidden cells, and changes to active scheduler deadlines. Startup tests register the quoted executable and remove its value under an isolated HKCU test key using the production helper, then verify that a neighboring value survived. A Windows sign-out/sign-in cycle and physical secondary or mixed-DPI monitors remain untested.

Created src/ui/SettingsWindow.h/.cpp, Settings.rc, SettingsIds.h and src/utils/Startup.h/.cpp. Modified CMakeLists.txt, src/config/ConfigManager.cpp, src/core/Application.cpp, src/monitor/MonitorEngine.h/.cpp, src/taskbar/OverlayWindow.h/.cpp, src/ui/Renderer.h/.cpp and MetricDisplay.h/.cpp. Updated FoundationTests.cpp, MonitorTests.cpp, DisplayTests.cpp, DesktopSmoke.ps1 and all three project documentation files.

## Phase 3 · Live overlay

Debug and Release each passed all four CTest suites (display, monitoring, placement and foundation). The stable live CPU-temperature-only configuration recorded zero changes and zero paints over ten seconds. The original Phase 0 mode also passed its stable-pixel and interaction checks.

The Windows 11 Release overlay displayed all eight fields at 144 DPI. The captured taskbar showed download 2.7 KB/s, upload 683 B/s, CPU 8%, GPU 1%, RAM 83%, both temperatures N/A and ping 36 ms. The live rectangle occupied (162,1135)–(678,1192) on the 1920 × 1200 desktop. The tested configuration enabled both temperatures temporarily. The original configuration was restored.

Over 60.02 seconds with the bar visible, Windows recorded 46.88 ms of application CPU time (0.0781% of one core). Working set was 24.39 → 24.45 MiB and private bytes 9.25 → 9.29 MiB. There were 59 paints and 59 visible text changes. Resident memory remains above the 15 MB target. These are short-run measurements under the observed desktop load.

Click repositioning preserved focus, a duplicate launch exited cleanly, and auto-hide retract/reveal passed. Explorer restarted from PID 17644 to 11876. Overlay PID 21796 recovered at its original rectangle and shut down cleanly.

Created src/ui/MetricDisplay.h/.cpp and tests/DisplayTests.cpp. Modified src/core/Application.cpp, src/monitor/Metrics.h, src/monitor/MonitorEngine.h, src/taskbar/OverlayWindow.h/.cpp, src/taskbar/Placement.h/.cpp, src/ui/Renderer.h/.cpp, tests/DesktopSmoke.ps1, CMakeLists.txt and the three project documentation files. Collector implementations are shared with Phase 2. No dependency was added.

The display tests cover the eight labels and units, hidden fields and fixed ordering, rounding, genuine zero, N/A, timeout, narrow dimensions and 150% pixel placement. Physical validation was limited to a single display. Live mixed-DPI transitions and behavior with alternate taskbar layouts still require hardware coverage.

## Phase 2 · Independent monitoring engine

Normal startup runs configured collectors and exposes coherent snapshots to in-process consumers. The console harness enables all collectors, displays readings and reports timing. The desktop regression still uses the original static overlay.

| Live check on Windows build 26300.9974 | Observed result |
| --- | --- |
| Physical network | One active Intel Wi-Fi 6E AX211. Comparable samples produced 152–782 B/s down and 46–422 B/s up in the eight-snapshot run. Multiple interfaces, disappearance, reconnection and wrap were exercised with synthetic counters. |
| CPU and physical RAM | CPU reported 4–15% after warm-up. RAM reported 81%. |
| WDDM GPU | Intel Iris Xe, driver 31.0.101.4575. Diagnostics grouped 24 engines and reported 0–1% rounded utilization on LUID eac5. An independent Windows PDH query enumerated 371 process instances, including nonzero samples for physical GPU 0, engine 0, type 3D. |
| Temperatures | The WDDM query supplied no usable GPU reading. Four Win32_TemperatureProbe instances had empty CurrentReading. CPU package temperature is unsupported. |
| ICMP | The default target returned 26–31 ms. The monitoring tests exercised DNS and echo on localhost, invalid input, timeout/cancellation status mapping, and shutdown. |
| Overlay and recovery | Normal mode passed duplicate launch, pointer focus, reposition, stable pixels, auto-hide and clean shutdown. Explorer PID 8496 restarted as PID 17644. Original overlay PID 19316 returned to the replacement taskbar. Development mode also passed its desktop smoke test. |

Release with the normal configuration used 109.38 ms CPU over 60.01 seconds (0.1823% of one core). Working set moved from 23.77 to 23.71 MiB and private bytes from 9.14 to 9.04 MiB. The overlay recorded zero additional paints. Phase 1 measured 0.0260%, 18.91 MiB resident and approximately 4.2 MiB private. These short runs include system and driver variability. Resident memory exceeds the 15 MB target.

In the separate eight-snapshot diagnostic with temperatures enabled, mean collector wall times in microseconds were network 1,501, CPU 117, RAM 4, GPU 34,638, temperature 40 and ping 547. The GPU mean includes first-query initialization. Ping measures submission and completion processing. Network transit and DNS wait time are excluded. Instrumented dispatch and publication overhead totaled 17 microseconds over 23 reported worker wakeups, excluding sleep, kernel wakeup costs and the current publication.

Created Metrics, SystemCollectors, GpuMonitor, PingMonitor and MonitorEngine header/source pairs plus TemperatureMonitor.cpp in src/monitor. Added tests/MonitorTests.cpp and tests/MonitorDiagnostics.cpp. Modified CMakeLists.txt, Application.cpp, ConfigManager.cpp, README.md, DEVELOPMENT_STATUS.md and TECHNICAL_DECISIONS.md. New system import libraries are iphlpapi, ws2_32, pdh and gdi32. No new third-party dependency.

Debug and Release each passed three CTest suites covering monitoring, the original nine placement cases, and the foundation. Physical multi-adapter traffic, discrete or multiple GPU hardware, live driver removal, temperature-capable drivers, secondary displays and long-duration stability require further hardware coverage. The next phase is to render selected snapshot fields in the existing overlay.

## Phase 1 · Native application foundation

**2026-11-21.** Application lifecycle, scoped resource ownership, instance exclusion, configuration and debugger logging are implemented. The existing overlay behavior passed live regression testing on the same 150% display. Phase 2 remains future work.

### Changes

Created Application.h/.cpp and ApplicationContext.h under src/core, OverlayWindow.h/.cpp under src/taskbar, ConfigManager.h/.cpp under src/config, and Logger.h/.cpp and SingleInstanceManager.h/.cpp under src/utils. Created tests/FoundationTests.cpp and vendored third_party/nlohmann/json.hpp with LICENSE.MIT.

Modified src/main.cpp, src/ui/Renderer.h/.cpp, CMakeLists.txt, tests/DesktopSmoke.ps1 and all three project documentation files. Placement.h/.cpp and the nine placement cases retain their Phase 0 behavior. Shell discovery remains colocated with OverlayWindow because it has one caller and shares the callback lifecycle.

The application target is now TaskbarMonitor.exe. The --phase0-test switch provides the static feasibility checks with default positioning and suppressed persistence. Normal mode loads configuration, applies placement and font size, and saves on successful shutdown. Startup preference, theme, enabled metrics, refresh intervals, spacing and ping target are validated and stored for later phases.

### Build and verification

Release and Debug built with MSVC, Windows SDK 10.0.26100.0 and C++20. The C++20 path conversion warning discovered during the initial build was corrected. CTest runs both the nine-case placement suite and the foundation suite. Foundation tests exercise defaults, round-trip loading, missing and invalid fields, unknown-field preservation, unchanged-save suppression, malformed-file preservation, future versions, instance exclusion and lock release. A locked-destination test exercises failed atomic replacement.

The desktop script launched a second application process, verified its successful exit and checked that the original overlay remained. Both configurations passed actual pointer focus preservation, edge repositioning, stable sampled pixels, auto-hide retract/reveal and clean shutdown. Normal Debug startup exercised configuration creation and saving.

Release recovery replaced Explorer PID 2280 with PID 8496. The taskbar HWND changed from 131948 to 197488. Original overlay PID 5276 received TaskbarCreated and returned to (528,1147)–(678,1180).

| Configuration | Visible idle interval | CPU time | Single-core CPU | Working set | Private bytes | Additional paints |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Release | 60.00 s | 15.62 ms | 0.0260% | 18.91 → 18.91 MiB | 4.22 → 4.19 MiB | 0 |
| Debug | 5.01 s | 0 ms recorded | Below measurement resolution | 19.46 → 19.46 MiB | 4.16 → 4.16 MiB | 0 |

Release working set increased approximately 2.6 MiB from the final Phase 0 measurement and exceeds the 15 MB target. These are short samples on one machine.

### Limitations and next step

Instance exclusion is scoped to an interactive Windows session. A malformed or future-version file runs with validated defaults/recoverable fields and remains protected from saving. Changes from an external editor during execution are unsupported. Duplicate launch notification can be missed while the first instance is still creating its window, while mutex exclusion still holds.

Secondary monitors, mixed-DPI transitions, other Windows builds and long stability runs remain unverified. The recommended next phase is the independent monitoring engine, with collector calculation tests and snapshot publication.

## Phase 0 · Windows 11 taskbar feasibility

**2026-11-21.** The native x64 prototype built and ran on the development desktop. The same live overlay survived replacement of the Explorer process. The scope of this result is one Windows build and one physical display.

### Environment and build

The workspace started empty, without Git metadata. Initial inspection found Git but no MSVC, SDK or CMake. The signed Microsoft Build Tools installer supplied the following development tools.

| Component | Observed version |
| --- | --- |
| Windows 11 x64 | 26H2, build 26300.9974 |
| Visual Studio Build Tools 2022 | 17.14.45 |
| MSVC x64 compiler | 19.44.35230.0, v143 toolset directory 14.44.35207 |
| Windows SDK | 10.0.26100.0 |
| CMake and CTest | 3.31.6-msvc6 |
| MSBuild | 17.14.60 |
| Other tools | Ninja 1.12.1, Git 2.55.0.windows.3 |

The CMake Visual Studio generator produced a 126,464-byte Release executable. DUMPBIN identified an x64 Windows GUI image importing USER32, SHELL32, D2D1, DWrite and KERNEL32. No third-party runtime was linked. The SHA-256 of the tested executable is 396A2BA4E41F99717833319090B039DFE1121A9162B42ABC80D8BEF72DF1B353.

### Desktop and automated checks

| Check | Evidence |
| --- | --- |
| Discovery | One primary horizontal taskbar on a 1920 × 1200 display at 144 DPI (150%). No secondary taskbar was present. |
| Auto-hide | The revealed taskbar occupied (0,1128)–(1920,1200). The retracted rectangle extended to y=1270, leaving two on-screen pixels. The sample hid during retraction and returned on reveal. |
| Placement | The visible sample occupied (528,1147)–(678,1180), fully inside the taskbar. Hit-testing found the overlay at its center. Switching edges moved it to (1242,1147)–(1392,1180). |
| Focus | After the other traffic monitor closed, a real pointer click on the sample switched edges and preserved the foreground window. Launch also preserved focus. |
| Rendering | A cropped desktop capture showed the static label between the left region and the centered app buttons. Twenty samples of the settled overlay at 100 ms intervals contained identical pixels. |
| Restart | Explorer PID 10164 was replaced by PID 2280. The primary taskbar HWND changed from 65772 to 131948. Overlay PID 21292 received TaskbarCreated and returned to its original rectangle. |
| Shutdown | The right-button release handler closed the test instance with exit code 0. |
| Geometry tests | CTest passed 1/1. Nine cases cover 100%, 125%, 150%, 200%, opposite edges, clamping, negative monitor origin, a clipped or sliver-sized taskbar, a vertical or narrow bar and missing DPI. |

### Resource measurements

Samples used the Release build after warm-up with the overlay visible. Working set includes shared resident pages. Private bytes measure committed private memory. The process CPU counter reported zero additional milliseconds in the intervals below. Small amounts of work can fall below its accounting resolution.

| Direct2D target | Interval | Working set, start → end | Private bytes, start → end | Additional paints |
| --- | ---: | ---: | ---: | ---: |
| Default target, initial comparison | 30.01 s | 51.96 → 51.98 MiB | 39.32 → 39.33 MiB | 0 |
| Software target, run including restart after sampling | 60.01 s | 16.19 → 16.23 MiB | 3.93 → 3.92 MiB | 2 |
| Software target, other traffic monitor closed | 60.01 s | 16.34 → 16.32 MiB | 3.84 → 3.80 MiB | 0 |

The software target substantially reduced resident and private memory. The measured working set still exceeded the initial 15 MB target. These short samples cannot establish long-term memory growth or predict the cost of collectors.

An early desktop test sampled changing pixels immediately after showing the window. The idle pixel test now starts 750 ms after first paint. Its result covers the settled overlay. Explorer's reveal transition and sub-100 ms changes were outside the pixel sampling check.

### Files and remaining coverage

All project files were created in Phase 0. There were no pre-existing source files to modify.

| Files | Purpose |
| --- | --- |
| CMakeLists.txt, .gitignore | Build targets and local build output exclusion |
| src/main.cpp | Native lifecycle, discovery, positioning, input and recovery |
| src/taskbar/Placement.h and .cpp | DPI-scaled rectangle calculation |
| src/ui/Renderer.h and .cpp | Direct2D and DirectWrite rendering |
| tests/PlacementTests.cpp, tests/DesktopSmoke.ps1 | Geometry and interactive desktop checks |
| README.md, TECHNICAL_DECISIONS.md, DEVELOPMENT_STATUS.md | Build/run instructions, architecture and measured status |
| build/Release/TaskbarMonitorPhase0.exe | Tested executable |

Secondary and mixed-DPI displays, a live scaling change, exclusive fullscreen and other Windows builds still need physical testing. Placement is configurable because the shell does not guarantee a free rectangle. During shell or display transitions the one-second discovery fallback can briefly lag a missed notification. Phase 1 and the full application's 24-hour stability test are future work.
