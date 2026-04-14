#include "MonitorEngine.h"
#include "Logging.h"
#include <cmath>
#include <cstdlib>

namespace uvg {

void MonitorEngine::Start() {
    Transition(EngineState::WaitingStartupDelay);
    Emit();
}

void MonitorEngine::Stop() {
    adlx_.UnsubscribeTuningEvents();
    adlx_.Terminate();
}

void MonitorEngine::Transition(EngineState next) {
    if (snap_.state != next) {
        Logger::Instance().Info(std::wstring(L"State: ") + ToString(snap_.state) +
                                L" -> " + ToString(next));
        snap_.state = next;
    }
}

void MonitorEngine::Emit() {
    snap_.adlxConnected = adlx_.IsInitialized();
    if (onUpdate_) onUpdate_(snap_);
}

void MonitorEngine::OnStartupDelayElapsed() {
    Transition(EngineState::WaitingForADLX);
    OnAdlxRetry();
}

void MonitorEngine::OnAdlxRetry() {
    if (adlx_.Initialize()) {
        Transition(EngineState::Monitoring);
        // First check immediately, then rely on event listener + poll.
        PerformCheck();
    }
    Emit();
}

void MonitorEngine::OnAdlxEvent() {
    Logger::Instance().Debug(L"ADLX event received");
    suspectStreak_ = 0;
    healthyStreak_ = 0;
    PerformCheck();
}

void MonitorEngine::OnPollTick() {
    PerformCheck();
}

bool MonitorEngine::IsStrictMismatch(const GpuState& s) const {
    if (s.voltageSupported) {
        if (std::abs(s.voltageMv - cfg_.targetVoltageMv) >
            cfg_.allowedVoltageDeviationMv) return true;
    }
    if (s.freqSupported) {
        if (std::abs(s.minFreqMHz - cfg_.targetMinFreqMHz) >
            cfg_.allowedMinFreqDeviationMHz) return true;
        if (std::abs(s.maxFreqMHz - cfg_.targetMaxFreqMHz) >
            cfg_.allowedMaxFreqDeviationMHz) return true;
    }
    return false;
}

int MonitorEngine::ComputeVoltagePercent(const GpuState& s) const {
    if (!s.voltageSupported) return 0;
    int span = s.rangeVoltMax - s.rangeVoltMin;
    if (span <= 0) return 0;
    int p = (int)(((long long)(s.voltageMv - s.rangeVoltMin) * 100) / span);
    if (p < 0) p = 0; if (p > 100) p = 100;
    return p;
}

void MonitorEngine::PerformCheck() {
    if (!adlx_.IsInitialized()) {
        Transition(EngineState::WaitingForADLX);
        Emit();
        return;
    }
    GpuState s;
    if (!adlx_.ReadState(s)) {
        Logger::Instance().Warn(L"ReadState failed");
        Emit();
        return;
    }
    snap_.last          = s;
    snap_.lastCheckTime = NowTimestamp();
    snap_.featureUnsupported = !(s.freqSupported || s.voltageSupported);
    if (snap_.featureUnsupported) {
        Transition(EngineState::Monitoring);
        Emit();
        return;
    }

    snap_.strictMismatch        = IsStrictMismatch(s);
    snap_.voltagePercentInRange = ComputeVoltagePercent(s);
    snap_.heuristicSlip         = (snap_.voltagePercentInRange >= cfg_.slippedPercentThreshold);

    const bool slipped = snap_.strictMismatch || snap_.heuristicSlip;

    if (slipped) {
        healthyStreak_ = 0;
        ++suspectStreak_;
        if (suspectStreak_ >= kDebounceConfirmCount) {
            Transition(EngineState::SlipSuspected);
            if (cfg_.autoReapplyEnabled) {
                bool ok = false;
                Transition(EngineState::Reapplying);
                if (reapply_.TryReapply(cfg_, ok)) {
                    Sleep(300); // brief settle - acceptable on UI thread
                    GpuState s2;
                    bool reread = adlx_.ReadState(s2);
                    bool good   = reread && !IsStrictMismatch(s2) &&
                                  ComputeVoltagePercent(s2) < cfg_.slippedPercentThreshold;
                    snap_.last              = reread ? s2 : s;
                    snap_.lastReapplyResult = good ? L"recovered" : L"failed";
                    if (good) {
                        Transition(EngineState::Healthy);
                        suspectStreak_ = 0;
                    } else {
                        Transition(EngineState::FailedToRecover);
                        if (snap_.adlxConnected) {
                            // Ask UI to show alert window
                            // (UI owns the AlertWindow lifetime)
                        }
                    }
                } else {
                    snap_.lastReapplyResult = L"cooldown";
                    Transition(EngineState::FailedToRecover);
                }
            } else {
                Transition(EngineState::FailedToRecover);
            }
        }
    } else {
        suspectStreak_ = 0;
        ++healthyStreak_;
        if (healthyStreak_ >= kDebounceConfirmCount) {
            Transition(EngineState::Healthy);
        } else if (snap_.state != EngineState::Healthy) {
            Transition(EngineState::Monitoring);
        }
    }
    Emit();
}

bool MonitorEngine::ApplyNow() {
    if (!adlx_.IsInitialized()) return false;
    bool ok = adlx_.ApplyTarget(cfg_.targetMinFreqMHz,
                                cfg_.targetMaxFreqMHz,
                                cfg_.targetVoltageMv);
    snap_.lastReapplyResult = ok ? L"manual ok" : L"manual failed";
    Sleep(300);
    PerformCheck();
    return ok;
}

bool MonitorEngine::CaptureAsTarget() {
    if (!adlx_.IsInitialized()) return false;
    GpuState s;
    if (!adlx_.ReadState(s)) return false;
    if (s.freqSupported) {
        cfg_.targetMinFreqMHz = s.minFreqMHz;
        cfg_.targetMaxFreqMHz = s.maxFreqMHz;
    }
    if (s.voltageSupported) {
        cfg_.targetVoltageMv = s.voltageMv;
    }
    cfg_.Clamp();
    return true;
}

} // namespace uvg
