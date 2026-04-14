#pragma once
#include "Common.h"
#include "Config.h"
#include "AdlxFacade.h"
#include "MonitorEngine.h"
#include "UiMainWindow.h"
#include <memory>

namespace uvg {

// Top-level application object. Holds singletons-by-composition: config,
// ADLX facade, monitor engine, main window. Owns lifetime, ensures
// single-instance, drives the message loop.
class App {
public:
    explicit App(HINSTANCE hInst) : hInst_(hInst) {}
    ~App();

    // Returns process exit code. Returns immediately with 0 if another
    // instance is already running and brings that one to front.
    int Run();

private:
    bool AcquireSingleInstance();
    void ReleaseSingleInstance();

    HINSTANCE hInst_      = nullptr;
    HANDLE    instMutex_  = nullptr;
    Config    cfg_{};
    std::unique_ptr<AdlxFacade>    adlx_;
    std::unique_ptr<MonitorEngine> engine_;
    std::unique_ptr<UiMainWindow>  win_;
};

} // namespace uvg
