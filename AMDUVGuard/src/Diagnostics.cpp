#include "Diagnostics.h"
#include <fstream>
#include <sstream>

namespace uvg {

std::wstring Diagnostics::BuildText(const AdlxFacade& adlx,
                                    const EngineSnapshot& snap,
                                    const Config& cfg) {
    std::wstringstream w;
    w << L"AMDUVGuard diagnostics\n"
      << L"  generated:        " << NowTimestamp() << L"\n"
      << L"  app version:      " << kAppVersion << L"\n"
      << L"  ADLX initialized: " << (adlx.IsInitialized() ? L"yes" : L"no") << L"\n"
      << L"  GPU name:         " << adlx.GetGpuName() << L"\n"
      << L"  Driver path:      " << adlx.GetDriverVersion() << L"\n"
      << L"  Event listener:   " << (adlx.EventListenerAttached() ? L"attached" : L"detached") << L"\n"
      << L"  Last ADLX error:  " << adlx.GetLastError() << L"\n"
      << L"\n"
      << L"  state:            " << ToString(snap.state) << L"\n"
      << L"  unsupported:      " << (snap.featureUnsupported ? L"yes" : L"no") << L"\n"
      << L"  last check:       " << snap.lastCheckTime << L"\n"
      << L"  last reapply:     " << snap.lastReapplyResult << L"\n"
      << L"  strict mismatch:  " << (snap.strictMismatch ? L"yes" : L"no") << L"\n"
      << L"  heuristic slip:   " << (snap.heuristicSlip ? L"yes" : L"no") << L"\n"
      << L"  voltage %inrange: " << snap.voltagePercentInRange << L"\n"
      << L"\n"
      << L"  current min/max/V: " << snap.last.minFreqMHz << L" / "
                                  << snap.last.maxFreqMHz << L" / "
                                  << snap.last.voltageMv << L"\n"
      << L"  range freq:        " << snap.last.rangeFreqMin << L"-" << snap.last.rangeFreqMax << L"\n"
      << L"  range volt:        " << snap.last.rangeVoltMin << L"-" << snap.last.rangeVoltMax << L"\n"
      << L"\n"
      << L"  target min/max/V:  " << cfg.targetMinFreqMHz << L" / "
                                  << cfg.targetMaxFreqMHz << L" / "
                                  << cfg.targetVoltageMv << L"\n"
      << L"  allowed dev MHz:   " << cfg.allowedMinFreqDeviationMHz << L"/"
                                  << cfg.allowedMaxFreqDeviationMHz << L"\n"
      << L"  allowed dev mV:    " << cfg.allowedVoltageDeviationMv << L"\n"
      << L"  slip percent:      " << cfg.slippedPercentThreshold << L"\n"
      << L"  startup delay:     " << cfg.startupDelaySeconds << L"s\n"
      << L"  poll interval:     " << cfg.pollIntervalSeconds  << L"s\n"
      << L"  auto reapply:      " << (cfg.autoReapplyEnabled ? L"yes" : L"no") << L"\n";
    return w.str();
}

bool Diagnostics::ExportToFile(const std::wstring& path,
                               const AdlxFacade& adlx,
                               const EngineSnapshot& snap,
                               const Config& cfg) {
    std::ofstream f(path, std::ios::trunc);
    if (!f.good()) return false;
    f << WideToUtf8(BuildText(adlx, snap, cfg));
    return true;
}

} // namespace uvg
