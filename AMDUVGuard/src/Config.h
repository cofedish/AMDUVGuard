#pragma once
#include "Common.h"
#include "Logging.h"

namespace uvg {

// User-editable configuration. Persisted as JSON in %AppData%\AMDUVGuard\config.json
struct Config {
    int  targetMinFreqMHz            = 500;
    int  targetMaxFreqMHz            = 2400;
    int  targetVoltageMv             = 1050;

    int  allowedMinFreqDeviationMHz  = 25;
    int  allowedMaxFreqDeviationMHz  = 25;
    int  allowedVoltageDeviationMv   = 10;

    int  slippedPercentThreshold     = kDefaultSlipPercent;
    int  startupDelaySeconds         = kDefaultStartupDelaySec;
    int  pollIntervalSeconds         = kDefaultPollIntervalSec;

    bool autoReapplyEnabled          = true;
    bool fullscreenAlertEnabled      = true;
    bool startWithWindows            = false;
    bool minimizeToTray              = true;

    LogLevel logLevel                = LogLevel::Info;

    // Sanitize ranges to safe values.
    void Clamp();

    // Returns config file path under %AppData%\AMDUVGuard\config.json
    static std::wstring DefaultPath();

    // Loads from disk; on failure, creates defaults and writes them.
    static Config LoadOrCreate(const std::wstring& path);
    bool Save(const std::wstring& path) const;
};

} // namespace uvg
