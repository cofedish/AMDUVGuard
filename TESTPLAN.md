# AMDUVGuard - manual test plan

| # | Scenario | Steps | Expected |
|---|----------|-------|----------|
| 1 | First launch, no config | Delete `%AppData%\AMDUVGuard\config.json`. Run app. | App starts, `config.json` is created with defaults. Status pane shows `WaitingStartupDelay`. |
| 2 | Startup delay honored | Set `startupDelaySeconds=20`, restart app. | For ~20s state stays `WaitingStartupDelay`, then transitions to `WaitingForADLX`. |
| 3 | ADLX not ready | Stop AMD driver before launch (or run on system w/o Adrenalin). | State `WaitingForADLX`. Log shows retries every ~20s. No CPU spam. No error popups. |
| 4 | ADLX ready, values match | Configure AMD Software, set the same values as target, run. | State quickly reaches `Healthy`. Strict mismatch and heuristic slip both `no`. |
| 5 | Slipped undervolt | Open AMD Software, change voltage by +50 mV, click apply. | App detects mismatch, transitions through `SlipSuspected → Reapplying → Healthy`. Last reapply shows `recovered`. |
| 6 | Reapply succeeds | Same as #5. | Healthy state, no alert window. |
| 7 | Reapply fails | Force fail (e.g. by manually editing `AdlxFacade::ApplyTarget` to return false in a debug build). | State `FailedToRecover`, topmost alert window shown with three buttons. |
| 8 | Unsupported GPU API | Run on a GPU where `IsSupportedManualGFXTuning` returns false. | UI shows "GPU tuning API: UNSUPPORTED". App does not crash, no apply attempts. |
| 9 | Single instance | Launch a second time. | The second process exits immediately and the first window is brought to front. |
| 10 | Minimize to tray | Click minimize or close (with `minimizeToTray=true`). | Window hides, tray icon present. Right-click → Show / Exit work. Left-click restores. |
| 11 | Sleep / resume | Put PC to sleep, wake it up. | After resume the timer fires next tick; if AMD reset undervolt, app reapplies. No crash. |
| 12 | Adrenalin restart | Kill and relaunch AMD Software. | ADLX event listener stays attached or app re-initializes via retry timer. State recovers without manual intervention. |
| 13 | Driver TDR / reset | Force a TDR (`dxgkrnl` reset). | App detects mismatch on next event/poll, attempts reapply. |
| 14 | Save / Reload | Change values, Save, restart app. | Values persist. Reload reverts unsaved edits. |
| 15 | "Use current values as target" | Click button while ADLX is connected. | Edit fields update to current ADLX values. |
| 16 | "Apply target now" | Click button. | ADLX values change to target. State refreshes. |
| 17 | "Test warn" | Click button. | Topmost alert window appears, buttons work. |
| 18 | Export diagnostics | Click button, save file. | Text file contains GPU name, state, current/target/range values. |
| 19 | Start with Windows | Enable checkbox, Save, log out, log in. | After logon delay, app launches automatically (Task Scheduler). |
| 20 | Disable Start with Windows | Uncheck, Save. | Task removed; `schtasks /Query /TN "AMDUVGuard Autostart"` returns nothing. |
| 21 | Log rotation | Set log level to Debug, run for a while. | Old `amduvguard.1.log` etc. are created at ~1 MB. No unbounded growth. |
| 22 | No idle CPU | Leave running idle for 10 minutes. | Process Explorer / Task Manager show ~0% CPU. Working set stable. |
| 23 | Clean exit | Right-click tray → Exit. | Process exits cleanly, no leaked window/timer/handle (verify with Process Explorer). |
