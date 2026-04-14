#include "App.h"
#include "Logging.h"

namespace uvg {

App::~App() {
    win_.reset();
    engine_.reset();
    adlx_.reset();
    Logger::Instance().Shutdown();
    ReleaseSingleInstance();
}

bool App::AcquireSingleInstance() {
    instMutex_ = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!instMutex_) return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Bring existing instance to front
        HWND existing = FindWindowW(kMainWndClass, nullptr);
        if (existing) {
            ShowWindow(existing, SW_SHOW);
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return false;
    }
    return true;
}

void App::ReleaseSingleInstance() {
    if (instMutex_) {
        ReleaseMutex(instMutex_);
        CloseHandle(instMutex_);
        instMutex_ = nullptr;
    }
}

int App::Run() {
    if (!AcquireSingleInstance()) return 0;

    cfg_ = Config::LoadOrCreate(Config::DefaultPath());
    Logger::Instance().Init(GetAppDataDir(), cfg_.logLevel);
    Logger::Instance().Info(std::wstring(L"AMDUVGuard ") + kAppVersion + L" starting");

    adlx_   = std::make_unique<AdlxFacade>();
    engine_ = std::make_unique<MonitorEngine>(*adlx_, cfg_,
        [this](const EngineSnapshot& s) {
            if (win_) win_->OnEngineUpdate(s);
        });

    win_ = std::make_unique<UiMainWindow>(hInst_, cfg_, *adlx_, *engine_);
    if (!win_->Create()) {
        Logger::Instance().Error(L"Window creation failed");
        return 1;
    }

    win_->Run();

    Logger::Instance().Info(L"AMDUVGuard exiting");
    return 0;
}

} // namespace uvg
