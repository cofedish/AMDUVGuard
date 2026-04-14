#include "ReapplyController.h"
#include "Logging.h"

namespace uvg {

ULONGLONG ReapplyController::SecondsSinceLastAttempt() const {
    if (lastAttemptTick_ == 0) return UINT64_MAX;
    return (GetTickCount64() - lastAttemptTick_) / 1000ULL;
}

bool ReapplyController::TryReapply(const Config& cfg, bool& appliedOk) {
    const ULONGLONG now = GetTickCount64();
    if (lastAttemptTick_ != 0 &&
        (now - lastAttemptTick_) < (ULONGLONG)kReapplyCooldownSec * 1000ULL) {
        return false; // cooldown
    }
    lastAttemptTick_ = now;
    Logger::Instance().Info(L"Reapply: attempt");
    appliedOk = adlx_.ApplyTarget(cfg.targetMinFreqMHz,
                                  cfg.targetMaxFreqMHz,
                                  cfg.targetVoltageMv);
    if (!appliedOk) {
        Logger::Instance().Warn(L"Reapply: ADLX rejected target");
    }
    return true;
}

} // namespace uvg
