# TrafficMonitor 1.86 recovery.3

Maintenance build of the TrafficMonitor 1.86 source tree. The application still displays its base 1.86 version; `recovery.3` identifies this fork's maintenance release.

## Changes

- Recover after repeated hardware/network acquisition errors: log native and managed exception details, stop acquisition cleanly, wait for the previous process to exit and allow a 5-second driver settle period before restarting.
- Limit automatic restarts to three per ten-minute window, retain pending retries, and continue acquisition if replacement process creation fails.
- Safely resize/validate network tables before sampling. Handle adapter disappearance, changed row mappings and bounded interface descriptions.
- Use monotonic sampling time; preserve the previous day's traffic when rolling over to a new date.
- Give network details its own table/snapshot and prevent background IP queries from dereferencing a closed dialog.
- Allow an exact zero-pixel taskbar left edge when widget avoidance is disabled. Preserve explicitly enabled widget avoidance and user offsets.
- Refresh taskbar placement after settings/widget/geometry changes; fix secondary-monitor coordinate conversion and Explorer child-window fallback handling.
- Retry failed window moves and test placement caching, touch-taskbar geometry and automatic bounds.

## Validation

Release x64 builds and the local regression suite pass. Tests include hardware-recovery scheduling, real replacement-process creation/exit, network buffer/error cases, 150 vertical layouts and 42 monitor-coordinate combinations. Native runtime loading was smoke-tested from an extracted portable package in an earlier maintenance revision. No real driver update or comprehensive GUI/plugin soak test is claimed.

## Portable download

Extract the entire ZIP to a writable folder, then run `TrafficMonitor.exe` with its normal administrator permission. Windows 10/11 x64 and .NET Framework 4.7.2+ are required. Native Visual C++/MFC release runtimes, skins and languages are included. Personal configuration, logs and traffic history are not included.

Do not extract over an existing installation without backing up its configuration/history. SHA256 checksum and per-file manifest accompany the portable package.

## Build from source

Install Visual Studio 2022 C++ tools, MFC, C++/CLI and .NET Framework 4.7.2 targeting support, then run from the repository root:

```powershell
powershell.exe -NoProfile -File maintenance\Build-Release.ps1
```

The scripts discover Visual Studio and redistributable directories instead of requiring a specific edition or toolset patch path. Generated package/build/test directories are local artifacts, not source files to commit.
