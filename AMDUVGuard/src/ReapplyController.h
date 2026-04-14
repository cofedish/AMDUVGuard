#pragma once
#include "Common.h"
#include "AdlxFacade.h"
#include "Config.h"

namespace uvg {

// Rate-limited target re-application.
//
// Guarantees:
//   - At most one full apply attempt per kReapplyCooldownSec.
//   - One attempt = SetMin -> SetMax -> SetVoltage -> small wait -> ReadState.
//   - Caller decides whether the read result counts as success.
class ReapplyController {
public:
    explicit ReapplyController(AdlxFacade& adlx) : adlx_(adlx) {}

    // Returns true if a re-apply attempt was made.
    // Sets `appliedOk` to whether ADLX accepted the writes.
    // If cooldown is active, returns false and leaves `appliedOk` untouched.
    bool TryReapply(const Config& cfg, bool& appliedOk);

    void Reset() { lastAttemptTick_ = 0; }

    ULONGLONG SecondsSinceLastAttempt() const;

private:
    AdlxFacade& adlx_;
    ULONGLONG   lastAttemptTick_ = 0;
};

} // namespace uvg
