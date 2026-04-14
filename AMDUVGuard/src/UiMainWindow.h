#pragma once
#include "Common.h"
#include <shellapi.h>
#include <vector>
#include "Config.h"
#include "MonitorEngine.h"
#include "AdlxFacade.h"
#include "AlertWindow.h"

namespace uvg {

// Main Win32 window: status panel + settings fields + tray icon.
//
// Threading: created on, and confined to, the UI thread. ADLX events are
// marshalled here as WM_APP_ADLX_EVENT. Polling is driven by a Win32
// SetTimer; no worker threads are spawned.
class UiMainWindow {
public:
    UiMainWindow(HINSTANCE hInst,
                 Config& cfg,
                 AdlxFacade& adlx,
                 MonitorEngine& engine);
    ~UiMainWindow();

    bool Create();
    void Run();      // standard message loop
    void Destroy();

    HWND Hwnd() const { return hwnd_; }

    // Engine -> UI: refresh status pane from snapshot
    void OnEngineUpdate(const EngineSnapshot& snap);

private:
    static LRESULT CALLBACK StaticProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT Proc(UINT, WPARAM, LPARAM);

    void BuildControls();
    void LayoutControls();
    void LoadFromConfig();
    void StoreToConfig();
    void RefreshStatusText(const EngineSnapshot& snap);

    void StartStartupDelayTimer();
    void StartAdlxRetryTimer();
    void StartPollTimer();
    void KillAllTimers();

    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowFromTray();
    void MinimizeToTray();
    void HandleTrayMessage(LPARAM lParam);

    void DoApplyNow();
    void DoCaptureCurrent();
    void DoSave();
    void DoReload();
    void DoTestWarning();
    void DoExportDiagnostics();

    void MaybeShowAlertFromState(const EngineSnapshot& snap);

    HINSTANCE      hInst_  = nullptr;
    HWND           hwnd_   = nullptr;
    Config&        cfg_;
    AdlxFacade&    adlx_;
    MonitorEngine& engine_;
    AlertWindow    alert_;

    HFONT hFontUi_      = nullptr;   // Segoe UI 10
    HFONT hFontLabel_   = nullptr;   // Segoe UI 9  — вторичные подписи
    HFONT hFontTitle_   = nullptr;   // Segoe UI Semibold 20 — баннер
    HFONT hFontSection_ = nullptr;   // Segoe UI Semibold 9 — заголовки секций
    HFONT hFontValue_   = nullptr;   // Segoe UI Semibold 10 — значения
    HFONT hFontMono_    = nullptr;   // Consolas 10 (не используется, резерв)
    HFONT hFontHelp_    = nullptr;   // Segoe UI 10 — подсказка
    ULONG_PTR gdiplusToken_ = 0;
    HBRUSH hBrBg_       = nullptr;   // фон окна
    HBRUSH hBrCard_     = nullptr;   // фон карточки
    HBRUSH hBrEdit_     = nullptr;   // фон поля ввода
    HBRUSH hBrBanner_   = nullptr;   // фон баннера, пересоздаётся
    int    bannerBg_    = 0;
    int    bannerFg_    = 0;
    std::vector<RECT> cardRects_;    // рисуются в WM_PAINT
    std::vector<int>  cardHeaderY_;  // под текст заголовка

    // Пересчёт положения всех дочерних окон под текущий client rect.
    void DoLayout();

    // controls
    HWND hBanner_  = nullptr;       // цветной баннер состояния
    HWND hStatus_  = nullptr;

    // Заголовки карточек
    HWND hSec_[5] = {};
    // Лейблы полей
    HWND hLblTgt_[3]  = {};
    HWND hLblDev_[3]  = {};
    HWND hLblPct_     = nullptr;
    HWND hLblStartup_ = nullptr;
    HWND hLblPoll_    = nullptr;
    // Кнопки
    HWND hBtnCapture_ = nullptr;
    HWND hBtnApply_   = nullptr;
    HWND hBtnSave_    = nullptr;
    HWND hBtnReload_  = nullptr;
    HWND hBtnTest_    = nullptr;
    HWND hBtnDiag_    = nullptr;
    HWND hBtnMin_     = nullptr;
    // Подсказка
    HWND hHelp_       = nullptr;
    HWND hEditTargetMin_   = nullptr;
    HWND hEditTargetMax_   = nullptr;
    HWND hEditTargetVolt_  = nullptr;
    HWND hEditDevMin_      = nullptr;
    HWND hEditDevMax_      = nullptr;
    HWND hEditDevVolt_     = nullptr;
    HWND hEditPercent_     = nullptr;
    HWND hEditStartupDelay_= nullptr;
    HWND hEditPoll_        = nullptr;
    HWND hChkAuto_         = nullptr;
    HWND hChkAlert_        = nullptr;
    HWND hChkStartup_      = nullptr;
    HWND hChkTray_         = nullptr;

    NOTIFYICONDATAW nid_{};
    bool trayAdded_ = false;
    bool quitting_  = false;
};

} // namespace uvg
