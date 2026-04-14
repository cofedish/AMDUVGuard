#pragma once
#include "Common.h"
#include <atomic>
#include <functional>
#include <string>

// We deliberately do NOT forward-declare any ADLX types here. The ADLX SDK
// puts its interfaces in `namespace adlx` but its helper class at file scope,
// and stray forward declarations cause "ambiguous symbol" errors. All ADLX
// types live behind the pimpl in AdlxFacade.cpp.

namespace uvg {

// Snapshot of GPU tuning state.
struct GpuState {
    int  minFreqMHz   = 0;
    int  maxFreqMHz   = 0;
    int  voltageMv    = 0;

    int  rangeFreqMin = 0;
    int  rangeFreqMax = 0;
    int  rangeVoltMin = 0;
    int  rangeVoltMax = 0;

    bool freqSupported    = false;
    bool voltageSupported = false;
};

// AdlxFacade - the only place that touches the AMD ADLX SDK.
//
// Threading: all public methods are intended to be called from the UI thread.
// The internal ADLX event listener may fire on a worker thread, in which case
// we PostMessage WM_APP_ADLX_EVENT to the UI thread sink window. The facade
// itself never spawns its own threads.
class AdlxFacade {
public:
    AdlxFacade();
    ~AdlxFacade();

    AdlxFacade(const AdlxFacade&) = delete;
    AdlxFacade& operator=(const AdlxFacade&) = delete;

    // Attempts to load adlx.dll, init helper, query first GPU and tuning service.
    // Returns true on success. Idempotent.
    bool Initialize();

    // Releases all ADLX interfaces in correct order. Safe to call multiple times.
    void Terminate();

    bool IsInitialized() const { return initialized_; }

    // Subscribes to GPU tuning changed events. When fired, posts
    // WM_APP_ADLX_EVENT to `sink`. Safe no-op if not initialized.
    bool SubscribeTuningEvents(HWND sink);
    void UnsubscribeTuningEvents();

    // Reads current GPU tuning state into `out`. Returns false on read failure.
    bool ReadState(GpuState& out);

    // Applies target values. Order: min freq -> max freq -> voltage.
    // Returns false if any step failed (lastError will be set).
    bool ApplyTarget(int minFreqMHz, int maxFreqMHz, int voltageMv);

    // Diagnostics
    std::wstring GetGpuName()      const { return gpuName_; }
    std::wstring GetDriverVersion()const { return driverVersion_; }
    std::wstring GetLastError()    const { return lastError_; }
    bool         EventListenerAttached() const { return eventAttached_; }

private:
    void SetError(const std::wstring& e);

    bool         initialized_   = false;
    bool         eventAttached_ = false;
    std::wstring gpuName_;
    std::wstring driverVersion_;
    std::wstring lastError_;

    // Opaque pimpl pointer to avoid including any ADLX header in this file.
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace uvg
