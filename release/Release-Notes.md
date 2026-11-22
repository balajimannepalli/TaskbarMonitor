# Taskbar Monitor 1.0.2

Appearance now includes a **Transparent background** option. The taskbar shows through behind the readings, and the text retains its configured color.

Left now starts flush with the taskbar. The previous default inset of 352 DIPs migrates to zero. Center aligns the overlay to the selected taskbar midpoint, and Right aligns it before the notification area. Custom insets continue to apply to the edge anchors. Both editions include the native taskbar overlay, eight selectable metrics and the Win32 settings editor. Unsupported temperature readings appear as N/A.

- **Setup** installs the application under Program Files, creates a Start Menu shortcut, and offers a desktop shortcut and current-user sign-in registration. Exit the installed monitor and run Setup again to upgrade. Settings survive upgrades and silent uninstall.
- **Portable** runs from a writable extracted folder. Its settings live in Data/config.json and its startup checkbox is disabled.
- Both editions use the static MSVC runtime and the Windows graphics, networking and performance-counter libraries. The nlohmann JSON license is included.

The executables are unsigned. Results from the development computer are documented in DEVELOPMENT_STATUS.md in the source project. Clean-VM operation, physical mixed-DPI transitions, sign-out/sign-in and 24-hour stability remain unverified. The measured working set exceeded 15 MiB.
