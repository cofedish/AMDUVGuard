#pragma once
#include "Common.h"
#include "AdlxFacade.h"
#include "Config.h"
#include "ReapplyController.h"
#include "StateMachine.h"
#include <functional>

namespace uvg {

// Public observable snapshot of engine state for the UI.
struct EngineSnapshot {
    EngineState  state         = EngineState::WaitingStartupDelay;
    bool         adlxConnected = false;
    GpuState     last;
    bool         strictMismatch         = false;
    bool         heuristicSlip          = false;
    int          voltagePercentInRange  = 0;   // 0..100
    std::wstring lastCheckTime;
    std::wstring lastReapplyResult;
    bool         featureUnsupported     = false;
};

// MonitorEngine - state machine driver. Lives on the UI thread.
//
// External inputs (all expected on UI thread):
//   Start()                  - kicks off WaitingStartupDelay
//   OnStartupDelayElapsed()  - move to WaitingForADLX, try Initialize
//   OnAdlxRetry()            - retry Initialize
//   OnPollTick()             - perform a check (also called on ADLX event)
//   OnAdlxEvent()            - alias for OnPollTick with debounce reset
//   ApplyNow()               - manual apply, bypasses cooldown
//   Stop()                   - clean shutdown
//
// The engine emits state changes via the OnUpdate callback so the UI can
// refresh without polling the engine itself.
class MonitorEngine {
public:
    using UpdateCb = std::function<void(const EngineSnapshot&)>;

    MonitorEngine(AdlxFacade& adlx, Config& cfg, UpdateCb cb)
        : adlx_(adlx), cfg_(cfg), reapply_(adlx), onUpdate_(std::move(cb)) {}

    void Start();
    void Stop();

    void OnStartupDelayElapsed();
    void OnAdlxRetry();
    void OnPollTick();
    void OnAdlxEvent();

    // Manually apply target now (UI button). Bypasses the cooldown timer.
    bool ApplyNow();

    // Use current ADLX values as the new target. Returns false if read failed.
    bool CaptureAsTarget();

    const EngineSnapshot& Snapshot() const { return snap_; }

private:
    void   Transition(EngineState next);
    void   Emit();
    void   PerformCheck();
    bool   IsStrictMismatch(const GpuState& s) const;
    int    ComputeVoltagePercent(const GpuState& s) const;

    AdlxFacade&       adlx_;
    Config&           cfg_;
    ReapplyController reapply_;
    UpdateCb          onUpdate_;
    EngineSnapshot    snap_;

    int suspectStreak_ = 0;
    int healthyStreak_ = 0;
};

} // namespace uvg
