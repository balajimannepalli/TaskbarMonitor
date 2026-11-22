# Taskbar Monitor 1.0.2

Taskbar Monitor displays selected live metrics on the Windows 11 taskbar. Left-click the readings to switch edges. Right-click for Settings or Exit Taskbar Monitor. The down and up arrows show receive and transmit rates. CPU, GPU and RAM show utilization. Enable cpuTemperature and gpuTemperature under enabledMetrics to display both temperatures. Unsupported or missing readings show N/A. Ping timeouts show Timeout.

## Install on Windows 11 x64

Run **TaskbarMonitor-Setup-x64.exe** from the release directory and approve the Windows elevation prompt. Setup installs to `C:\Program Files\Taskbar Monitor` by default and creates a Start Menu shortcut. Choose the optional desktop shortcut and sign-in registration during setup. On the final page, select **Launch Taskbar Monitor**, or open it later from Start. The release is unsigned. Visual Studio, CMake and the Visual C++ redistributable are unnecessary on the target PC.

To upgrade, exit the installed monitor from its overlay menu and run the new Setup. The installer keeps `%LOCALAPPDATA%\TaskbarMonitor\config.json` and the current startup choice. Change startup later in Settings. Sign-in registration belongs to the Windows account that selected it.

To uninstall, exit the monitor, then open **Settings → Apps → Installed apps → Taskbar Monitor → Uninstall**. The uninstaller removes the program, its shortcuts and the matching Run entry for the account performing removal. Choose **No** at the settings-file prompt to keep your configuration. Silent uninstall keeps it automatically. If other Windows accounts enabled startup, turn it off in their Settings before uninstalling.

## Run the portable edition

Extract **TaskbarMonitor-Portable-x64.zip** to a writable folder and open **TaskbarMonitor.exe** there. Its configuration is `Data\config.json` inside that folder. Keep Data when moving or upgrading the portable copy. Portable startup registration is disabled. The installed and portable editions can run together with separate settings. Each edition allows one instance per Windows session.

See [release artifacts and SHA-256 checksums](release/) and [release notes](release/Release-Notes.md). To verify a download, run `Get-FileHash .\TaskbarMonitor-Setup-x64.exe -Algorithm SHA256` (or supply the ZIP name) and compare it with SHA256SUMS.txt.

## Settings

Right-click the live overlay and choose **Settings**. You can also run the executable with **--settings** to open the dialog in the running instance, including when the chosen monitor is disconnected or every metric is hidden.

Select a metric, change its **Visible** checkbox and use **Move up** or **Move down** to set the display order. Choose System, Light, Dark or Custom for the overlay. Custom accepts six-digit hexadecimal text and background colors. Enter an installed font family and size in DIPs. Compact reduces column spacing and row height. Padding controls the margin around the text. Enable **Transparent background** to show the taskbar behind the readings. The selected theme or custom text color still applies.

Choose Left for the left edge, Center for the midpoint of the selected taskbar, or Right for the space immediately before the notification area. Left and Right accept an inward inset in DIPs. Zero is flush. Center ignores the inset. Vertical offsets are clamped to keep readings inside a horizontal taskbar. Refresh intervals are in milliseconds and accept 250–3,600,000.

**Save** validates the draft, persists it and applies it to the running application. **Cancel** discards edits. **Restore Defaults** changes the draft and takes effect on Save. The sign-in option registers the quoted executable path under the current user's Windows Run key. Use the executable from the location where you intend to keep it.

The development build includes a console diagnostic. From the project directory, run the following command to display eight one-second snapshots and a timing summary. It reads the configured intervals, ping target and timeout, and enables all collectors for inspection. Output ends automatically and shutdown drains pending requests.

~~~powershell
.\build\Release\MonitorDiagnostics.exe 8
~~~

Each field prints its value and unit or a status such as pending, unsupported, unavailable, or timeout. Network units use decimal prefixes. A hostname must resolve to IPv4 for the current ping collector. The default target is 1.1.1.1, with one probe every two seconds and a one-second timeout. Run the diagnostic separately to measure the application in isolation.

## Build and run

Use an x64 Native Tools or Developer PowerShell prompt for Visual Studio 2022 with the C++ desktop build tools, a Windows 11 SDK and the C++ CMake tools installed. The application runs as the current user.

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --build build --config Debug
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Debug --output-on-failure
.\build\Release\TaskbarMonitor.exe
~~~

To regenerate the distributions, build both Release targets and run the following from the repository root with Inno Setup 6 installed. The release script reads product metadata from the built executable, compiles Setup, archives the portable build, and writes checksums.

~~~powershell
.\packaging\Build-Packages.ps1 -BuildDirectory .\build -Compiler "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
~~~

The live overlay stacks two readings per column. It sizes itself to the enabled fields, font and spacing, and positions the entire rectangle at the selected anchor. The development sample retains its 100 × 22 DIP rectangle. Check the available space on your own taskbar before launching. Close the previous instance before trying another position.

~~~powershell
.\build\Release\TaskbarMonitor.exe --right --inset 260
.\build\Release\TaskbarMonitor.exe --monitor '\\.\DISPLAY2' --inset 120
~~~

The monitor option accepts a Windows display device name. Omit it to follow the primary taskbar. If the selected taskbar disappears, the process waits with its window hidden and attaches when the bar returns. To quit while hidden, reveal the selected bar and right-click the sample, then choose Exit Taskbar Monitor. Exit codes are 0 for a normal close, 1 for DPI or argument initialization, 2 for invalid arguments, 3 for window or graphics initialization/message-loop failure, and 4 for an unrecoverable paint error.

## Repeat the desktop checks

Run the smoke test at ordinary desktop privilege with other prototype instances closed. It launches its own instance, checks on-screen placement and mouse activation behavior, moves the sample, times idle CPU and paint calls, measures memory, and closes its instance.

~~~powershell
.\tests\DesktopSmoke.ps1
~~~

For an auto-hidden primary bottom taskbar, add **-RevealBottomTaskbar**. This temporarily moves the pointer and exercises a retract/reveal cycle. Leave the pointer idle while it runs. The optional capture directory must already exist. It receives two cropped taskbar screenshots.

For a five-minute live stability sample, run the following command on the interactive desktop. It checks a missing monitor and explicitly selects the primary monitor. During sampling it reports handle and thread counts, working set, private bytes, process and UI-thread CPU, paints, displayed changes and scheduler time. The display and shell notification checks send messages only to the test window. Actual shell replacement is a separate test.

~~~powershell
.\tests\DesktopSmoke.ps1 -NormalMode -SelectPrimaryMonitor -RevealBottomTaskbar -Seconds 300
~~~

~~~powershell
.\tests\DesktopSmoke.ps1 -RevealBottomTaskbar -CaptureDirectory .\build
~~~

The live restart check ends the process that owns this session's primary taskbar. Save any File Explorer work and finish shell file transfers before using this switch. Explorer may close folder windows. The test waits for a replacement shell, checks for a real TaskbarCreated broadcast, and verifies that the original overlay is visible over the replacement taskbar.

~~~powershell
.\tests\DesktopSmoke.ps1 -RevealBottomTaskbar -RestartExplorer
~~~

See [development status](DEVELOPMENT_STATUS.md) for measured results and untested configurations. The [technical decisions](TECHNICAL_DECISIONS.md) describe the integration and its limits.

## Configuration and lifecycle

The application loads %LOCALAPPDATA%\TaskbarMonitor\config.json. A normal successful close saves validated settings through a temporary file and atomic replacement. An unchanged file incurs no write. Missing fields receive defaults, and unknown JSON fields survive a save. Malformed JSON, files larger than 1 MiB, and future schema versions are preserved on disk. Fix such files manually to resume persistence. Initialization or persistence failures return exit code 5 and emit a debugger error message.

The version 1 schema contains startup, theme, enabledMetrics, metricOrder, refreshIntervals, overlay, fontFamily, fontSize, textColor, backgroundColor, metricSpacing, padding, compact, pingTarget and pingTimeoutMs. Normal startup applies placement, font size, collector enablement, sampling intervals and ping configuration. The timeout accepts 100–5,000 ms. Sampling intervals accept 250–3,600,000 ms. Font size is 8–18 DIPs, spacing is 0–64 DIPs, and inset is 0–16,384 DIPs. Edit the file while the application is closed.

Normal left-click positioning persists on clean exit. Command-line position overrides apply for that run. A second instance exits successfully after requesting a non-activating geometry refresh from the original process. Instance exclusion is scoped to the Windows session.

The explicit **--phase0-test** switch restores the original test defaults and suppresses saving. The desktop test script uses this switch by default. Add **-NormalMode** to exercise normal persistence. The original static label and paint counters remain available for development verification.

The source vendors nlohmann JSON 3.12.0 under its MIT license. This header-only dependency handles JSON parsing, validation primitives, UTF-8 and serialization. Build and execution require no dependency download or additional runtime. The license is included under third_party/nlohmann.



