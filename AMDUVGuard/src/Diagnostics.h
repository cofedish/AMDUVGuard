#pragma once
#include "Common.h"
#include "AdlxFacade.h"
#include "MonitorEngine.h"
#include "Config.h"

namespace uvg {

// Builds a human-readable diagnostics blob and writes it to disk on demand.
class Diagnostics {
public:
    static std::wstring BuildText(const AdlxFacade& adlx,
                                  const EngineSnapshot& snap,
                                  const Config& cfg);

    static bool ExportToFile(const std::wstring& path,
                             const AdlxFacade& adlx,
                             const EngineSnapshot& snap,
                             const Config& cfg);
};

} // namespace uvg
