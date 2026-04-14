#pragma once
#include "Common.h"

namespace uvg {

// Top-level engine state. Transitions are linear except slip/recover loop.
enum class EngineState {
    WaitingStartupDelay = 0,
    WaitingForADLX      = 1,
    Monitoring          = 2,
    SlipSuspected       = 3,
    Reapplying          = 4,
    Healthy             = 5,
    FailedToRecover     = 6,
};

inline const wchar_t* ToString(EngineState s) {
    switch (s) {
        case EngineState::WaitingStartupDelay: return L"WaitingStartupDelay";
        case EngineState::WaitingForADLX:      return L"WaitingForADLX";
        case EngineState::Monitoring:          return L"Monitoring";
        case EngineState::SlipSuspected:       return L"SlipSuspected";
        case EngineState::Reapplying:          return L"Reapplying";
        case EngineState::Healthy:             return L"Healthy";
        case EngineState::FailedToRecover:     return L"FailedToRecover";
    }
    return L"?";
}

} // namespace uvg
