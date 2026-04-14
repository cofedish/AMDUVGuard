// AdlxFacade - implementation against the official AMD ADLX SDK.
//
// This translation unit is the ONLY place that includes ADLX headers.
// It uses the public ADLX interfaces:
//   IADLXSystem, IADLXGPUList, IADLXGPU,
//   IADLXGPUTuningServices, IADLXManualGraphicsTuning2,
//   IADLXGPUTuningChangedHandling, IADLXGPUTuningChangedEvent,
//   IADLXGPUTuningChangedListener,
//   IADLXFrequencyRange, IADLXIntRange.
//
// Build requirement: define UVG_HAVE_ADLX (preprocessor) and add the ADLX
// SDK include path. Without UVG_HAVE_ADLX the facade compiles as a stub
// reporting "ADLX SDK not available" - this lets the rest of the project
// build and run on machines without the SDK while you wire it up.

#include "AdlxFacade.h"
#include "Logging.h"

#ifdef UVG_HAVE_ADLX
  #include "ADLXHelper/Windows/Cpp/ADLXHelper.h"
  #include "Include/IGPUTuning.h"
  #include "Include/IGPUManualGFXTuning.h"
  // ADLX interfaces live in `namespace adlx`; the helper class itself is at
  // file scope. Bring the interface namespace in for convenience.
  using namespace adlx;
#endif

namespace uvg {

struct AdlxFacade::Impl {
#ifdef UVG_HAVE_ADLX
    ADLXHelper                                    helper;
    IADLXGPUPtr                                   gpu;
    IADLXGPUTuningServicesPtr                     tuning;
    IADLXInterfacePtr                             gfxIface;        // raw manual GFX tuning iface
    IADLXManualGraphicsTuning2Ptr                 gfx;
    IADLXGPUTuningChangedHandlingPtr              evtHandling;
    // Listener implemented below as a member to keep lifetime tied to Impl.
    class Listener;
    Listener*                                     listener = nullptr;
#endif
    HWND sinkHwnd = nullptr;
};

#ifdef UVG_HAVE_ADLX
class AdlxFacade::Impl::Listener : public IADLXGPUTuningChangedListener {
public:
    explicit Listener(HWND hwnd) : hwnd_(hwnd) {}
    adlx_bool ADLX_STD_CALL OnGPUTuningChanged(IADLXGPUTuningChangedEvent* /*evt*/) override {
        if (hwnd_ && IsWindow(hwnd_))
            PostMessageW(hwnd_, WM_APP_ADLX_EVENT, 0, 0);
        return true; // keep listening
    }
private:
    HWND hwnd_;
};
#endif

AdlxFacade::AdlxFacade() : impl_(new Impl()) {}

AdlxFacade::~AdlxFacade() {
    Terminate();
    delete impl_;
    impl_ = nullptr;
}

void AdlxFacade::SetError(const std::wstring& e) {
    lastError_ = e;
    Logger::Instance().Warn(L"ADLX: " + e);
}

bool AdlxFacade::Initialize() {
    if (initialized_) return true;

#ifndef UVG_HAVE_ADLX
    SetError(L"ADLX SDK not compiled in (UVG_HAVE_ADLX undefined)");
    return false;
#else
    ADLX_RESULT res = impl_->helper.Initialize();
    if (ADLX_FAILED(res)) { SetError(L"ADLXHelper.Initialize failed"); return false; }

    IADLXSystem* sys = impl_->helper.GetSystemServices();
    if (!sys) { SetError(L"GetSystemServices returned null"); return false; }

    IADLXGPUListPtr gpus;
    res = sys->GetGPUs(&gpus);
    if (ADLX_FAILED(res) || !gpus) { SetError(L"GetGPUs failed"); return false; }

    res = gpus->At(gpus->Begin(), &impl_->gpu);
    if (ADLX_FAILED(res) || !impl_->gpu) { SetError(L"No GPU at list[0]"); return false; }

    const char* nameA = nullptr;
    if (ADLX_SUCCEEDED(impl_->gpu->Name(&nameA)) && nameA)
        gpuName_ = Utf8ToWide(nameA);

    const char* drvA = nullptr;
    if (ADLX_SUCCEEDED(impl_->gpu->DriverPath(&drvA)) && drvA)
        driverVersion_ = Utf8ToWide(drvA);

    res = sys->GetGPUTuningServices(&impl_->tuning);
    if (ADLX_FAILED(res) || !impl_->tuning) {
        SetError(L"GetGPUTuningServices failed");
        return false;
    }

    adlx_bool supported = false;
    res = impl_->tuning->IsSupportedManualGFXTuning(impl_->gpu, &supported);
    if (ADLX_FAILED(res) || !supported) {
        SetError(L"Manual GFX tuning not supported on this GPU");
        // Still mark initialized so UI can show "unsupported"
        initialized_ = true;
        return true;
    }

    res = impl_->tuning->GetManualGFXTuning(impl_->gpu, &impl_->gfxIface);
    if (ADLX_FAILED(res) || !impl_->gfxIface) {
        SetError(L"GetManualGFXTuning failed");
        return false;
    }

    impl_->gfx = IADLXManualGraphicsTuning2Ptr(impl_->gfxIface);
    if (!impl_->gfx) {
        SetError(L"GPU does not implement IADLXManualGraphicsTuning2");
        initialized_ = true;
        return true;
    }

    initialized_ = true;
    Logger::Instance().Info(L"ADLX initialized: " + gpuName_);
    return true;
#endif
}

void AdlxFacade::Terminate() {
#ifdef UVG_HAVE_ADLX
    UnsubscribeTuningEvents();
    if (impl_) {
        impl_->gfx          = nullptr;
        impl_->gfxIface     = nullptr;
        impl_->tuning       = nullptr;
        impl_->gpu          = nullptr;
        impl_->evtHandling  = nullptr;
        if (initialized_) impl_->helper.Terminate();
    }
#endif
    initialized_   = false;
    eventAttached_ = false;
}

bool AdlxFacade::SubscribeTuningEvents(HWND sink) {
#ifndef UVG_HAVE_ADLX
    (void)sink; return false;
#else
    if (!initialized_ || !impl_->tuning) return false;
    if (eventAttached_) return true;

    ADLX_RESULT res = impl_->tuning->GetGPUTuningChangedHandling(&impl_->evtHandling);
    if (ADLX_FAILED(res) || !impl_->evtHandling) {
        SetError(L"GetGPUTuningChangedHandling failed");
        return false;
    }
    impl_->sinkHwnd = sink;
    impl_->listener = new Impl::Listener(sink);
    res = impl_->evtHandling->AddGPUTuningEventListener(impl_->listener);
    if (ADLX_FAILED(res)) {
        SetError(L"AddGPUTuningEventListener failed");
        delete impl_->listener; impl_->listener = nullptr;
        return false;
    }
    eventAttached_ = true;
    return true;
#endif
}

void AdlxFacade::UnsubscribeTuningEvents() {
#ifdef UVG_HAVE_ADLX
    if (impl_ && impl_->evtHandling && impl_->listener) {
        impl_->evtHandling->RemoveGPUTuningEventListener(impl_->listener);
        delete impl_->listener;
        impl_->listener = nullptr;
    }
#endif
    eventAttached_ = false;
}

bool AdlxFacade::ReadState(GpuState& out) {
#ifndef UVG_HAVE_ADLX
    (void)out; return false;
#else
    if (!initialized_ || !impl_->gfx) return false;

    // IADLXManualGraphicsTuning2 has no per-feature IsSupported* methods;
    // support is gated at IADLXGPUTuningServices::IsSupportedManualGFXTuning.
    // Probe by attempting reads and treat per-call failure as unsupported.
    ADLX_IntRange freqRange{}, voltRange{};
    adlx_int curMin = 0, curMax = 0, curVolt = 0;

    bool freqOk =
        ADLX_SUCCEEDED(impl_->gfx->GetGPUMinFrequencyRange(&freqRange)) &&
        ADLX_SUCCEEDED(impl_->gfx->GetGPUMinFrequency(&curMin)) &&
        ADLX_SUCCEEDED(impl_->gfx->GetGPUMaxFrequency(&curMax));
    bool voltOk =
        ADLX_SUCCEEDED(impl_->gfx->GetGPUVoltageRange(&voltRange)) &&
        ADLX_SUCCEEDED(impl_->gfx->GetGPUVoltage(&curVolt));

    out.freqSupported    = freqOk;
    out.voltageSupported = voltOk;
    if (freqOk) {
        out.minFreqMHz   = (int)curMin;
        out.maxFreqMHz   = (int)curMax;
        out.rangeFreqMin = (int)freqRange.minValue;
        out.rangeFreqMax = (int)freqRange.maxValue;
    }
    if (voltOk) {
        out.voltageMv    = (int)curVolt;
        out.rangeVoltMin = (int)voltRange.minValue;
        out.rangeVoltMax = (int)voltRange.maxValue;
    }
    return freqOk || voltOk;
#endif
}

bool AdlxFacade::ApplyTarget(int minFreqMHz, int maxFreqMHz, int voltageMv) {
#ifndef UVG_HAVE_ADLX
    (void)minFreqMHz; (void)maxFreqMHz; (void)voltageMv; return false;
#else
    if (!initialized_ || !impl_->gfx) {
        SetError(L"ApplyTarget: not initialized");
        return false;
    }
    ADLX_RESULT r;
    r = impl_->gfx->SetGPUMinFrequency(minFreqMHz);
    if (ADLX_FAILED(r)) { SetError(L"SetGPUMinFrequency failed"); return false; }
    r = impl_->gfx->SetGPUMaxFrequency(maxFreqMHz);
    if (ADLX_FAILED(r)) { SetError(L"SetGPUMaxFrequency failed"); return false; }
    r = impl_->gfx->SetGPUVoltage(voltageMv);
    if (ADLX_FAILED(r)) { SetError(L"SetGPUVoltage failed"); return false; }
    Logger::Instance().Info(L"ADLX ApplyTarget OK");
    return true;
#endif
}

} // namespace uvg
