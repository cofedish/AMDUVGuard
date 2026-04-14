#pragma once
#include "Common.h"

namespace uvg {

// Task Scheduler integration for "Start with Windows" with logon delay.
// Uses schtasks.exe via XML import - avoids COM dependency on taskschd.h.
class SchedulerIntegration {
public:
    // Creates or replaces the per-user scheduled task.
    // delaySec is honored as PT{N}S in the task XML.
    static bool Install(const std::wstring& exePath, int delaySec);

    // Removes the task if present.
    static bool Uninstall();

    // Returns true if the task currently exists for the user.
    static bool IsInstalled();
};

} // namespace uvg
