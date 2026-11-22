# Taskbar Monitor technical decisions

## Phase 6 distribution

CMake owns the 1.0.0 product version and generates the Windows version resource. Both builds share the icon, settings resource, monitoring code and static runtime. Inno Setup reads the product version and publisher from the installed-mode executable. The stable AppId identifies the installation across versions. The installer contains the installed executable and the vendored JSON license, and writes shortcuts and uninstall metadata. User configuration is outside its payload.

The portable target builds the same application with a compile-time distribution flag. That flag resolves configuration relative to the executable, selects a separate session mutex and window class, and disables startup registry access. Renaming the portable executable or changing the working directory cannot switch it into installed mode. Two portable folders still share one portable instance per session.

Program Files installation requires elevation. The opt-in startup action runs the installed executable with the original user's token when Setup can obtain it. Upgrades skip that action. The uninstaller checks the current user's named value against the quoted installed executable before deleting it. Interactive settings cleanup defaults to keeping the file. On explicit deletion it checks the resolved path of an exclusively opened handle and marks that same file for deletion. It never recursively removes a profile directory. Silent uninstall preserves the configuration.

## Phase 5 hardening

The renderer keeps the last text associated with each layout. A changed cell rebuilds its DirectWrite layout and metrics. Shrinking the list releases the removed layouts. A new font starts with a fresh renderer. Spacing and painting use the existing draw path. A regression test counts actual layout creations while changing one value, repeating it, and removing a cell.

The GPU counter parser reads the named decimal and hexadecimal fields directly from a wide string view. It rejects signs, empty fields and unsigned overflow before aggregating engine identities. An x64 optimized microbenchmark with one million identical valid counter names took 697,381 µs with formatted scanning and 71,672 µs with the checked parser. This measures parsing alone. The application still performs the full PDH query and dynamically enumerates instances.

Loading and saving use the same 1 MiB byte limit. When pretty printing exceeds the limit, the saver tries compact JSON. If that also exceeds the limit, it reports failure before opening the temporary file. Boundary tests exercise an exact-limit compact round trip and rejection one byte above it.

External process samples include Windows and driver work charged to the process. Scheduler bookkeeping excludes the collectors and their waits. Working-set inspection counted resident pages and pages shared by more than one process at the time of sampling. Page trimming would affect that snapshot without eliminating the code or data the application needs.

## Phase 4 settings

The settings editor uses a Win32 dialog resource with native keyboard navigation and DPI-scaled dialog units. Its dialog loop continues dispatching overlay messages. It holds a private JSON draft. Save checks field ranges, RGB syntax and the installed DirectWrite font collection. A short-lived worker performs registry and atomic file writes. Publication to the UI follows successful persistence. Cancel and Restore Defaults can be used without disk or registry writes.

Schema version 1 gains additive fields for metric order, font family, custom RGB colors, padding, compact mode and vertical offset. Missing fields receive defaults. Invalid orders retain recognized unique keys in order and append missing metrics. Unknown JSON fields survive a save. Malformed and future-version file protection continues to apply.

The UI retains ownership of graphics resources. Visual changes rebuild the text layouts on the UI thread. Scheduler settings use a short shared lock and a separate wake event per worker. Each worker keeps its collector instances, copies updated settings and revises affected deadlines. A worker with no enabled jobs sleeps until a settings change or shutdown.

The previous release stored a startup preference only. The new helper reads and writes the TaskbarMonitor string value under HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run. Disabling deletes that named value. The editor reads actual registration when it opens. A failed config replacement after a registration change triggers a registration rollback. The registry and config file do not form a crash-atomic cross-store transaction.

## Phase 3 display

Application passes a read-only MetricsState pointer into the overlay for the lifetime of its message loop. The existing geometry watchdog and shell notifications also refresh visible text. A try-lock snapshot read lets the UI skip a contended update. Formatting compares the visible strings, excluding timestamps, hidden fields and differences below display precision. Only changes invalidate the text. Ordinary exposure, DPI changes and graphics recovery can also require painting.

The schema supports visibility and spacing. Field order is fixed. Cells fill columns from top to bottom in the order download, upload, CPU, GPU, RAM, CPU temperature, GPU temperature and ping. Valid values reuse the monitoring formatters. DirectWrite measures each string and caches the layouts between changes. Direct2D scales a measured cell uniformly when necessary. The original slot's inside edge anchors the live rectangle. Space below 54 DIPs per column or 12 DIPs per row suppresses display. All-hidden configurations also suppress the window. Explorer still provides no guarantee that this rectangle is free of buttons on another desktop.

## Phase 2 monitoring

The normal application scopes MonitorEngine around the overlay message loop. Development overlay mode skips engine startup. The engine depends on Windows collection APIs and the validated settings. The rendering code has no monitoring dependency. Each collector owns its baselines and resources. A mutex protects publication of whole groups of results. Readers copy a coherent snapshot with optional values, unit types, availability and monotonic timestamps of the last successful readings. Different fields can have different sample ages.

One deadline scheduler runs the fast system collectors and waits on the stop and ping-completion events. A second worker runs the same scheduling logic for enabled graphics collectors, because PDH initialization and graphics-driver queries can delay unrelated metrics. Each deadline advances to the next future multiple of its interval after a sample. Delayed samples generate one update. Stop signals interrupt waits. Workers join before engine destruction.

NetworkMonitor uses GetIfTable2 and keys counters by NET_LUID. The filter requires an up hardware Ethernet or 802.11 interface with no tunnel, filter, endpoint or disconnected flag. Driver-reported characteristics determine eligibility. Virtualized switches and unusual drivers can hide a physical path or report misleading characteristics. Each comparable adapter contributes its 64-bit byte differences divided by actual elapsed seconds. New or regressed counters establish a baseline. Absence drops the baseline, and a wrapped counter loses one interval. An aggregate with zero comparable interfaces is unavailable.

CPU busy time is kernel plus user minus idle, computed from GetSystemTimes deltas. Kernel time includes idle. Counter regressions, impossible idle deltas and zero denominators invalidate the interval. On systems with more than 64 logical processors, Windows can limit GetSystemTimes to the calling thread's primary processor group. GlobalMemoryStatusEx provides physical totals, available bytes and Windows' integer load estimate. The RAM percentage uses the unrounded used/total ratio.

The GPU query uses the PDH English wildcard path **\\GPU Engine(*)\\Utilization Percentage**. Each formatted array refresh discovers the current process instances. Their names contain adapter LUID, physical index and engine number. Valid per-process percentages for the same physical engine are added and capped at its 100% capacity. The busiest engine across adapters selects the GPU. Ties use the first ordered identity. An empty or failed sample clears the published reading. Query failures retry after a ten-second backoff.

GPU temperature queries the documented WDDM adapter performance data for the selected physical GPU. Before a GPU selection exists, it tries physical index zero of enumerated adapters. Positive sensor readings below 200 °C are converted from tenths of a degree. Missing or implausible data is unavailable. CPU package temperature is unsupported with the implemented interfaces. The development machine's generic temperature-probe records had no current readings or package attribution. [Microsoft adapter performance data](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_adapter_perfdata)

Ping supports one outstanding IPv4 request. Numeric targets bypass DNS. Hostnames use overlapped GetAddrInfoExW, followed by event-driven IcmpSendEcho2 and IcmpParseReplies. A missed ping deadline during an outstanding request skips that launch. Shutdown cancels pending DNS and drains its completion. An outstanding echo drains within its configured timeout under normal Windows operation. The reply buffer and event outlive the request.

## Phase 1 foundation

Application owns the startup and shutdown sequence. Its ApplicationContext scopes the instance mutex, configuration manager and overlay options. OverlayWindow contains the proven window procedure, shell discovery and event handling moved from main.cpp. Placement and Renderer retain their previous roles. The small entry point catches failures and returns a diagnostic exit code. Window destruction, event unhooking, mutex release and command-line allocation cleanup are scoped to owners.

The named session mutex is acquired before configuration loading or window creation. A duplicate can request geometry recovery from the existing window. A launch racing initial window creation still exits safely even if the refresh request cannot find a window. An abandoned mutex allows recovery after a process crash. The application never activates the overlay as a duplicate-launch response.

ConfigManager owns the schema defaults and field validation. Version 1 adds missing fields individually and preserves unknown fields. Invalid known fields receive defaults. Syntactically invalid and future-version files remain protected from writes. Preserving the source file supports manual recovery when syntax prevents reliable extraction. The single running application writes a same-directory temporary file, flushes it, and replaces the destination using MoveFileEx. Failure leaves the preceding configuration intact. External editors should be used while the app is closed.

The pinned nlohmann JSON 3.12.0 header replaces the need to maintain a custom parser. It adds compiled code only and introduces no worker, service or runtime DLL. The MIT license accompanies the vendored header. [Upstream release](https://github.com/nlohmann/json/releases/tag/v3.12.0)

Logger emits fixed development messages through OutputDebugString. INFO output is disabled in Release. WARNING and ERROR remain available to a debugger. Logging writes no files and includes no configuration contents or paths.

## Shell boundary and discovery

The prototype owns one top-level popup in its own process. Explorer retains ownership of the taskbar. Primary discovery uses the conventional Shell_TrayWnd window class. Explicit monitor selection enumerates top-level primary and Shell_SecondaryTrayWnd candidates and matches their monitor device names. These classes are Explorer implementation details that can change. The public appbar API can report the system taskbar rectangle but does not identify vacant space among its buttons or enumerate secondary taskbars. A user-selected inset and hiding on failed discovery are the conservative fallbacks. [Microsoft appbar API](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shappbarmessage)

## Visibility and recovery

The popup begins hidden and uses WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE, SWP_NOACTIVATE and MA_NOACTIVATE. Per-monitor V2 awareness is set before creating windows. The placement calculation uses physical screen rectangles and converts the inset and sample size from DIPs using the destination taskbar DPI. It rejects clipped, too-small and vertical taskbars. A monitor-covering foreground window above the bar suppresses the overlay. [Microsoft window positioning](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)

TaskbarCreated, display/settings notifications and DPI changes trigger fresh discovery. An out-of-context WinEvent hook listens for location changes in the selected taskbar process. Callback code executes in the prototype. A one-second timer checks geometry and z-order to recover from missed events, shell downtime and late secondary taskbars. It calls SetWindowPos only when placement, visibility or shell stacking requires it. Recovery requires no persistent Explorer handle. The prototype assumes a single interactive desktop and one selected taskbar at a time. [Microsoft taskbar creation notification](https://learn.microsoft.com/en-us/windows/win32/shell/taskbar#taskbar-creation-notification) · [Microsoft WinEvent delivery](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)

## Rendering and measurement

DirectWrite formats and Direct2D resources are cached. WM_PAINT draws the opaque background and sample in one pass. A lost render target is released and recreated on a subsequent paint. There is no frame timer. The smoke test reads successful paint counts and shell broadcast counts through read-only messages, and samples process CPU time, working set and private bytes externally. CPU percentage is CPU time divided by wall time, expressed on a single logical core. The test does not write continuously to disk.

The final build requests a Direct2D software HWND target. This kept the static label legible at the tested DPI and reduced the measured working set from about 52 MiB to 16 MiB. The x64 target uses the static MSVC runtime and Windows system DLLs. PowerShell and its inline Win32 interop support desktop testing only. Keeping lifecycle, options and discovery together in one source file limits Phase 0 to three application translation units. Geometry and graphics are separate so placement can be tested without opening a window.
