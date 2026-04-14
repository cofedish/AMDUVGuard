# AMDUVGuard

Lightweight Win32 desktop watchdog that monitors your manual AMD Radeon
undervolt and re-applies it through the official **AMD ADLX SDK** if it slips
(driver reset, AMD Software restart, sleep/resume, etc.).

- Pure Win32 + Common Controls. No Electron, no .NET, no Qt.
- Single instance, runs in the user session (not a service).
- Event-driven via ADLX `IADLXGPUTuningChangedHandling` plus a slow safety poll.
- Idle CPU usage is essentially zero.

## Architecture

```
App
 ├─ Config              (JSON in %AppData%\AMDUVGuard\config.json)
 ├─ Logging             (rotated log, 3 x 1 MB)
 ├─ AdlxFacade          (only TU that includes ADLX headers)
 ├─ MonitorEngine       (state machine; debounce; voltage % heuristic)
 │   └─ ReapplyController (cooldown-limited apply)
 ├─ UiMainWindow        (status pane, settings, tray, timers)
 │   └─ AlertWindow     (topmost borderless warning)
 ├─ SchedulerIntegration (Task Scheduler logon trigger w/ delay)
 └─ Diagnostics         (snapshot/export)
```

State machine:
`WaitingStartupDelay → WaitingForADLX → Monitoring → {Healthy | SlipSuspected → Reapplying → {Healthy | FailedToRecover}}`

## Build

Requirements:
- Visual Studio 2022 (`v143` toolset), Windows 10/11 SDK.
- The official AMD ADLX SDK: https://gpuopen.com/adlx/
  Download and unzip somewhere, e.g. `C:\sdk\ADLX`.

### One-time setup

Set the `ADLX_SDK_PATH` environment variable to the SDK include root,
for example:

```
setx ADLX_SDK_PATH "C:\sdk\ADLX\SDK"
```

Restart Visual Studio after this. The `.vcxproj` adds `$(ADLX_SDK_PATH)` to
the include path.

In **Project → Properties → C/C++ → Preprocessor → Preprocessor Definitions**
add `UVG_HAVE_ADLX` (Release|x64). Without this define the project still
builds and runs as a stub that reports "ADLX not available" — useful for
UI work on a non-AMD machine.

> The GitHub Actions workflow in `.github/workflows/build.yml` builds the
> stub configuration (no `UVG_HAVE_ADLX`), so CI does not require the
> proprietary ADLX SDK to be present on the runner.

You also need to add the ADLX helper sources to the build. The official
SDK ships them under `SDK\ADLXHelper\Windows\Cpp\`. Add
`ADLXHelper.cpp` to the project (right-click project → Add → Existing item).

### Build & run

1. Open `AMDUVGuard.sln`.
2. Select `Release | x64`.
3. `Build → Build Solution`.
4. Output: `bin\Release\AMDUVGuard.exe`.

## Run

First launch creates `%AppData%\AMDUVGuard\config.json` with defaults.
Open the app, set your **target min/max/voltage** to match the values you
configured in AMD Software, then press **Save**.

To capture your current AMD Software settings as the target automatically,
press **Use current**.

## Autostart

Use the **Start with Windows** checkbox in the UI. This creates a Task
Scheduler entry under your user account with a logon delay (the same value
as `startupDelaySeconds`), so the app starts well after the AMD driver is
ready. It is **not** placed in the Startup folder and does **not** create a
service.

## Logs & diagnostics

- Logs: `%AppData%\AMDUVGuard\amduvguard.log` (rotated, max 3 files).
- Diagnostics: **Export diagnostics** button writes a text snapshot.

## What it does NOT do

- No screen scraping or OCR of AMD Software.
- No UI automation, no clicking AMD Software.
- No process hooking.
- No network activity, no telemetry.
- Does not require admin unless you enable Task Scheduler autostart at
  highest available privileges.

## Files

```
AMDUVGuard.sln
AMDUVGuard/AMDUVGuard.vcxproj
AMDUVGuard/AMDUVGuard.exe.manifest
AMDUVGuard/src/
   Common.h               main.cpp           App.{h,cpp}
   Config.{h,cpp}         Logging.{h,cpp}
   AdlxFacade.{h,cpp}     StateMachine.h
   MonitorEngine.{h,cpp}  ReapplyController.{h,cpp}
   UiMainWindow.{h,cpp}   AlertWindow.{h,cpp}
   SchedulerIntegration.{h,cpp}
   Diagnostics.{h,cpp}
   AMDUVGuard.rc          resource.h
config.example.json
TESTPLAN.md
.gitignore
```

## Troubleshooting

- "ADLX not available" → ensure ADLX runtime is installed (ships with the
  Adrenalin driver) and that `UVG_HAVE_ADLX` was defined at build time.
- "Manual GFX tuning not supported" → your card / driver does not expose
  `IADLXManualGraphicsTuning2`. The app will keep monitoring but cannot
  read or apply.
- Status stuck on `WaitingStartupDelay` → that is intentional for the first
  N seconds after logon. Reduce `startupDelaySeconds` if you want.
