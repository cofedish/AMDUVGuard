// AMDUVGuard — главное окно. Светлая macOS-like тема.
// Карточки и кнопки рисуются через GDI+ со сглаживанием.
#include "UiMainWindow.h"
#include "Logging.h"
#include "Diagnostics.h"
#include "SchedulerIntegration.h"
#include "AlertWindow.h"
#include "resource.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <commdlg.h>
#include <strsafe.h>
#include <sstream>
#include <algorithm>

// GDI+ использует min/max из std — подавляем макросы из windows.h.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <objidl.h>
#include <gdiplus.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace uvg {

// ─── Светлая палитра, вдохновлённая macOS ──────────────────────────
namespace pal {
    constexpr COLORREF Bg          = RGB(242, 243, 247);
    constexpr COLORREF Card        = RGB(255, 255, 255);
    constexpr COLORREF CardBorder  = RGB(226, 228, 234);
    constexpr COLORREF Divider     = RGB(236, 238, 243);

    constexpr COLORREF Text        = RGB( 28,  28,  30);
    constexpr COLORREF Muted       = RGB(110, 113, 122);
    constexpr COLORREF Tertiary    = RGB(160, 163, 172);
    constexpr COLORREF Section     = RGB(125, 128, 138);

    constexpr COLORREF EditBg      = RGB(255, 255, 255);
    constexpr COLORREF EditBorder  = RGB(214, 216, 222);
    constexpr COLORREF EditFocus   = RGB( 10, 132, 255);

    constexpr COLORREF BtnBg       = RGB(246, 247, 250);
    constexpr COLORREF BtnBgHover  = RGB(238, 240, 246);
    constexpr COLORREF BtnBgPress  = RGB(228, 232, 240);
    constexpr COLORREF BtnBorder   = RGB(218, 222, 230);
    constexpr COLORREF BtnText     = RGB( 28,  28,  30);

    constexpr COLORREF Accent      = RGB( 10, 132, 255);
    constexpr COLORREF AccentHover = RGB( 51, 149, 255);
    constexpr COLORREF AccentPress = RGB(  0, 102, 224);
    constexpr COLORREF AccentText  = RGB(255, 255, 255);

    // Состояния (насыщенные, но не неоновые)
    constexpr COLORREF Healthy     = RGB( 52, 199,  89);   // зелёный
    constexpr COLORREF HealthyDk   = RGB( 34, 172,  66);
    constexpr COLORREF Warn        = RGB(255, 159,  10);   // янтарный
    constexpr COLORREF WarnDk      = RGB(230, 128,   0);
    constexpr COLORREF Danger      = RGB(255,  69,  58);   // коралловый
    constexpr COLORREF DangerDk    = RGB(224,  50,  40);
    constexpr COLORREF Info        = RGB( 91, 139, 255);   // спокойный синий
    constexpr COLORREF InfoDk      = RGB( 68, 118, 235);

    constexpr COLORREF BannerText  = RGB(255, 255, 255);
}

namespace {

constexpr UINT TRAY_UID  = 1;

// Раскладка
constexpr int  kPad          = 28;   // внешний отступ окна
constexpr int  kCardPad      = 24;   // внутренний паддинг карточки
constexpr int  kCardGap      = 18;   // расстояние между карточками
constexpr int  kRowH         = 40;   // высота строки формы
constexpr int  kLabelH       = 20;
constexpr int  kEditW        = 120;
constexpr int  kEditH        = 32;
constexpr int  kBtnH         = 46;
constexpr int  kBannerH      = 104;
constexpr int  kCardRadius   = 16;
constexpr int  kBannerRadius = 16;
constexpr int  kBtnRadius    = 11;
constexpr int  kEditRadius   = 9;
constexpr int  kSectionH     = 18;
constexpr int  kSectionGap   = 12;

// Окно
constexpr int  kWinW = 960;
constexpr int  kWinH = 1320;
constexpr int  kMinW = 880;
constexpr int  kMinH = 1100;

constexpr int  IDC_BTN_PRIMARY = IDC_BTN_SAVE;

// ─── GDI+ helpers ──────────────────────────────────────────────────
Gdiplus::GraphicsPath* RoundedPath(const Gdiplus::Rect& r, int radius) {
    int d = radius * 2;
    auto* p = new Gdiplus::GraphicsPath();
    p->AddArc(r.X,                     r.Y,                     d, d, 180, 90);
    p->AddArc(r.X + r.Width  - d,      r.Y,                     d, d, 270, 90);
    p->AddArc(r.X + r.Width  - d,      r.Y + r.Height - d,      d, d,   0, 90);
    p->AddArc(r.X,                     r.Y + r.Height - d,      d, d,  90, 90);
    p->CloseFigure();
    return p;
}

Gdiplus::Color FromCOLORREF(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// Мягкая тень под карточкой: несколько слоёв с убывающей прозрачностью.
void DrawSoftShadow(Gdiplus::Graphics& g, Gdiplus::Rect r, int radius) {
    for (int i = 6; i >= 1; --i) {
        Gdiplus::Rect sr(r.X - i, r.Y - i + 2, r.Width + 2 * i, r.Height + 2 * i);
        auto* p = RoundedPath(sr, radius + i);
        Gdiplus::SolidBrush br(Gdiplus::Color(14 - i * 2, 20, 24, 48));
        g.FillPath(&br, p);
        delete p;
    }
}

// ─── Шрифт helper ──────────────────────────────────────────────────
HFONT MakeFont(const wchar_t* face, int size, int weight = FW_NORMAL) {
    LOGFONTW lf{};
    lf.lfHeight  = -MulDiv(size, 96, 72);
    lf.lfWeight  = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    StringCchCopyW(lf.lfFaceName, LF_FACESIZE, face);
    return CreateFontIndirectW(&lf);
}

// ─── Subclass: hover / focus для кнопок и полей ────────────────────
LRESULT CALLBACK HoverSubclass(HWND h, UINT m, WPARAM w, LPARAM l,
                               UINT_PTR id, DWORD_PTR /*ref*/) {
    switch (m) {
    case WM_MOUSEMOVE:
        if (!GetPropW(h, L"hov")) {
            SetPropW(h, L"hov", (HANDLE)1);
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(h, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(h, L"hov");
        InvalidateRect(h, nullptr, FALSE);
        break;
    case WM_NCDESTROY:
        RemovePropW(h, L"hov");
        RemoveWindowSubclass(h, HoverSubclass, id);
        break;
    }
    return DefSubclassProc(h, m, w, l);
}

// Edit: перерисовать родителя при фокусе, чтобы рамка полей
// подсвечивалась синим.
LRESULT CALLBACK EditSubclass(HWND h, UINT m, WPARAM w, LPARAM l,
                              UINT_PTR id, DWORD_PTR /*ref*/) {
    switch (m) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        HWND parent = GetParent(h);
        RECT rc; GetWindowRect(h, &rc);
        MapWindowPoints(nullptr, parent, (POINT*)&rc, 2);
        InflateRect(&rc, 3, 3);
        InvalidateRect(parent, &rc, TRUE);
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(h, EditSubclass, id);
        break;
    }
    return DefSubclassProc(h, m, w, l);
}

HWND MkLabel(HWND parent, HINSTANCE hi, HFONT font, const wchar_t* t,
             int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE,
                             x, y, w, h, parent, nullptr, hi, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}
HWND MkEdit(HWND parent, HINSTANCE hi, HFONT font, int id,
            int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        ES_AUTOHSCROLL | ES_NUMBER | ES_CENTER,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, hi, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowSubclass(c, EditSubclass, 2, 0);
    return c;
}
HWND MkBtn(HWND parent, HINSTANCE hi, HFONT font, int id, const wchar_t* t,
           int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"BUTTON", t,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, hi, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowSubclass(c, HoverSubclass, 1, 0);
    return c;
}
HWND MkChk(HWND parent, HINSTANCE hi, HFONT font, int id, const wchar_t* t,
           int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"BUTTON", t,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, hi, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}
int GetEditInt(HWND e) {
    wchar_t buf[32]; GetWindowTextW(e, buf, 32);
    return _wtoi(buf);
}
void SetEditInt(HWND e, int v) {
    wchar_t buf[32]; StringCchPrintfW(buf, 32, L"%d", v);
    SetWindowTextW(e, buf);
}

void StateColors(EngineState s, COLORREF& c1, COLORREF& c2, std::wstring& title) {
    switch (s) {
    case EngineState::Healthy:
        c1 = pal::Healthy; c2 = pal::HealthyDk;
        title = L"Андервольт удерживается";
        break;
    case EngineState::FailedToRecover:
        c1 = pal::Danger; c2 = pal::DangerDk;
        title = L"Андервольт слетел и не восстановлен";
        break;
    case EngineState::SlipSuspected:
    case EngineState::Reapplying:
        c1 = pal::Warn; c2 = pal::WarnDk;
        title = L"Восстановление настроек…";
        break;
    case EngineState::WaitingStartupDelay:
        c1 = pal::Info; c2 = pal::InfoDk;
        title = L"Ожидание после входа в систему";
        break;
    case EngineState::WaitingForADLX:
        c1 = pal::Info; c2 = pal::InfoDk;
        title = L"Подключение к AMD ADLX";
        break;
    case EngineState::Monitoring:
    default:
        c1 = pal::Info; c2 = pal::InfoDk;
        title = L"Мониторинг активен";
        break;
    }
}

} // namespace

UiMainWindow::UiMainWindow(HINSTANCE hInst, Config& cfg, AdlxFacade& adlx, MonitorEngine& engine)
    : hInst_(hInst), cfg_(cfg), adlx_(adlx), engine_(engine) {}

UiMainWindow::~UiMainWindow() { Destroy(); }

bool UiMainWindow::Create() {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &gsi, nullptr);

    hFontUi_      = MakeFont(L"Segoe UI",          11, FW_NORMAL);
    hFontLabel_   = MakeFont(L"Segoe UI",          10, FW_NORMAL);
    hFontTitle_   = MakeFont(L"Segoe UI Semibold", 22, FW_SEMIBOLD);
    hFontSection_ = MakeFont(L"Segoe UI Semibold", 10, FW_SEMIBOLD);
    hFontValue_   = MakeFont(L"Segoe UI Semibold", 11, FW_SEMIBOLD);
    hFontMono_    = MakeFont(L"Consolas",          11, FW_NORMAL);
    hFontHelp_    = MakeFont(L"Segoe UI",          11, FW_NORMAL);

    hBrBg_   = CreateSolidBrush(pal::Bg);
    hBrCard_ = CreateSolidBrush(pal::Card);
    hBrEdit_ = CreateSolidBrush(pal::EditBg);

    HICON appIcon = (HICON)LoadImageW(hInst_, MAKEINTRESOURCEW(IDI_APPICON),
                                      IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc   = StaticProc;
    wc.hInstance     = hInst_;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = hBrBg_;
    wc.lpszClassName = kMainWndClass;
    wc.hIcon         = appIcon ? appIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = wc.hIcon;
    if (!RegisterClassExW(&wc)) return false;

    hwnd_ = CreateWindowExW(0, kMainWndClass, L"AMDUVGuard  —  страж андервольта",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, kWinW, kWinH,
        nullptr, nullptr, hInst_, this);
    if (!hwnd_) return false;

    // Светлая шапка (Win 11 / Win 10 1809+).
    BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));

    BuildControls();
    LoadFromConfig();
    AddTrayIcon();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    engine_.Start();
    StartStartupDelayTimer();
    return true;
}

void UiMainWindow::Destroy() {
    if (hwnd_) {
        KillAllTimers();
        adlx_.UnsubscribeTuningEvents();
        RemoveTrayIcon();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    auto del = [](HBRUSH& b){ if (b) { DeleteObject(b); b = nullptr; } };
    del(hBrBg_); del(hBrCard_); del(hBrEdit_); del(hBrBanner_);
    auto delF = [](HFONT& f){ if (f) { DeleteObject(f); f = nullptr; } };
    delF(hFontUi_); delF(hFontLabel_); delF(hFontTitle_); delF(hFontSection_);
    delF(hFontValue_); delF(hFontMono_); delF(hFontHelp_);
    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
}

void UiMainWindow::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

LRESULT CALLBACK UiMainWindow::StaticProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    UiMainWindow* self = nullptr;
    if (m == WM_NCCREATE) {
        self = reinterpret_cast<UiMainWindow*>(((CREATESTRUCT*)l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        self->hwnd_ = h;
    } else {
        self = reinterpret_cast<UiMainWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    return self ? self->Proc(m, w, l) : DefWindowProcW(h, m, w, l);
}

void UiMainWindow::BuildControls() {
    hBanner_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY,
        0, 0, 0, 0, hwnd_, (HMENU)(INT_PTR)(IDC_STATUS + 100),
        hInst_, nullptr);
    SendMessageW(hBanner_, WM_SETFONT, (WPARAM)hFontTitle_, TRUE);

    const wchar_t* kSecTitles[5] = {
        L"СОСТОЯНИЕ",
        L"ЦЕЛЕВЫЕ ЗНАЧЕНИЯ И ДОПУСКИ",
        L"ПОВЕДЕНИЕ",
        L"ДЕЙСТВИЯ",
        L"КАК ПОЛЬЗОВАТЬСЯ",
    };
    for (int i = 0; i < 5; ++i) {
        hSec_[i] = CreateWindowExW(0, L"STATIC", kSecTitles[i],
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, hInst_, nullptr);
        SendMessageW(hSec_[i], WM_SETFONT, (WPARAM)hFontSection_, TRUE);
    }

    hStatus_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, hwnd_, (HMENU)IDC_STATUS, hInst_, nullptr);
    SendMessageW(hStatus_, WM_SETFONT, (WPARAM)hFontUi_, TRUE);

    const wchar_t* kTgtLbl[3] = { L"Мин. частота, МГц", L"Макс. частота, МГц", L"Напряжение, мВ" };
    const wchar_t* kDevLbl[3] = { L"Δ мин. частоты",    L"Δ макс. частоты",    L"Δ напряжения" };
    for (int i = 0; i < 3; ++i) {
        hLblTgt_[i] = MkLabel(hwnd_, hInst_, hFontUi_, kTgtLbl[i], 0, 0, 0, 0);
        hLblDev_[i] = MkLabel(hwnd_, hInst_, hFontUi_, kDevLbl[i], 0, 0, 0, 0);
    }
    hEditTargetMin_  = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_TGT_MIN,  0, 0, 0, 0);
    hEditTargetMax_  = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_TGT_MAX,  0, 0, 0, 0);
    hEditTargetVolt_ = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_TGT_VOLT, 0, 0, 0, 0);
    hEditDevMin_     = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_DEV_MIN,  0, 0, 0, 0);
    hEditDevMax_     = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_DEV_MAX,  0, 0, 0, 0);
    hEditDevVolt_    = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_DEV_VOLT, 0, 0, 0, 0);

    hLblPct_     = MkLabel(hwnd_, hInst_, hFontUi_, L"Порог «слип» по напряжению, %", 0, 0, 0, 0);
    hLblStartup_ = MkLabel(hwnd_, hInst_, hFontUi_, L"Задержка после входа, с",       0, 0, 0, 0);
    hLblPoll_    = MkLabel(hwnd_, hInst_, hFontUi_, L"Период проверки, с",            0, 0, 0, 0);
    hEditPercent_      = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_PERCENT, 0, 0, 0, 0);
    hEditStartupDelay_ = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_STARTUP, 0, 0, 0, 0);
    hEditPoll_         = MkEdit(hwnd_, hInst_, hFontValue_, IDC_EDIT_POLL,    0, 0, 0, 0);
    hChkAuto_    = MkChk(hwnd_, hInst_, hFontUi_, IDC_CHK_AUTO,    L"Автоматически восстанавливать", 0, 0, 0, 0);
    hChkAlert_   = MkChk(hwnd_, hInst_, hFontUi_, IDC_CHK_ALERT,   L"Показывать заметное окно",      0, 0, 0, 0);
    hChkStartup_ = MkChk(hwnd_, hInst_, hFontUi_, IDC_CHK_STARTUP, L"Запускать вместе с Windows",    0, 0, 0, 0);
    hChkTray_    = MkChk(hwnd_, hInst_, hFontUi_, IDC_CHK_TRAY,    L"Сворачивать в трей",            0, 0, 0, 0);

    hBtnCapture_ = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_CAPTURE,  L"Взять текущие",  0, 0, 0, 0);
    hBtnApply_   = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_APPLY,    L"Применить цель", 0, 0, 0, 0);
    hBtnSave_    = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_SAVE,     L"Сохранить",      0, 0, 0, 0);
    hBtnReload_  = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_RELOAD,   L"Перезагрузить",  0, 0, 0, 0);
    hBtnTest_    = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_TESTWARN, L"Тест тревоги",   0, 0, 0, 0);
    hBtnDiag_    = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_DIAG,     L"Экспорт диагностики…", 0, 0, 0, 0);
    hBtnMin_     = MkBtn(hwnd_, hInst_, hFontUi_, IDC_BTN_MIN,      L"Свернуть в трей",      0, 0, 0, 0);

    hHelp_ = CreateWindowExW(0, L"STATIC",
        L"1.   Откройте AMD Software и выставьте свой андервольт вручную.\n"
        L"2.   Здесь нажмите «Взять текущие» — поля заполнятся значениями из ADLX.\n"
        L"3.   Нажмите «Сохранить» — программа будет следить за этими значениями.\n"
        L"4.   Если драйвер сбросит андервольт, баннер сверху станет жёлтым или красным, и программа сама применит нужные значения через ADLX.\n"
        L"5.   Чтобы утилита запускалась после входа в Windows, поставьте галочку «Запускать вместе с Windows» и снова нажмите «Сохранить».",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, hwnd_, nullptr, hInst_, nullptr);
    SendMessageW(hHelp_, WM_SETFONT, (WPARAM)hFontHelp_, TRUE);

    DoLayout();
}

void UiMainWindow::DoLayout() {
    if (!hwnd_) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    const int CW = rc.right - rc.left;
    const int CH = rc.bottom - rc.top;
    const int cx = CW - 2 * kPad;
    if (cx <= 0 || CH <= 0) return;

    int x = kPad;
    int y = kPad;

    cardRects_.clear();

    // 1. Баннер — фактически «карточка с цветной заливкой»
    SetWindowPos(hBanner_, nullptr, x, y, cx, kBannerH, SWP_NOZORDER);
    y += kBannerH + kCardGap + 8;

    auto Section = [&](int idx, int cardH) -> RECT {
        SetWindowPos(hSec_[idx], nullptr, x + 4, y, cx - 8, kSectionH, SWP_NOZORDER);
        y += kSectionH + kSectionGap;
        RECT cr{ x, y, x + cx, y + cardH };
        cardRects_.push_back(cr);
        y += cardH;
        return cr;
    };

    // 2. СОСТОЯНИЕ — таблица ключ/значение (6 строк по ~22px)
    const int statusH = 152;
    RECT c1 = Section(0, statusH + 2 * kCardPad - 16);
    SetWindowPos(hStatus_, nullptr,
                 c1.left + kCardPad, c1.top + kCardPad,
                 (c1.right - c1.left) - 2 * kCardPad, statusH, SWP_NOZORDER);
    y += kCardGap;

    // 3. ЦЕЛЕВЫЕ ЗНАЧЕНИЯ И ДОПУСКИ
    const int paramsH = kRowH * 3 + 2 * kCardPad - 4;
    RECT c2 = Section(1, paramsH);

    const int innerW = (c2.right - c2.left) - 2 * kCardPad;
    const int colGap = 28;
    const int colW   = (innerW - colGap) / 2;
    const int leftX  = c2.left + kCardPad;
    const int rightX = leftX + colW + colGap;
    int py = c2.top + kCardPad;

    auto PlaceRow = [&](HWND lblL, HWND edL, HWND lblR, HWND edR) {
        SetWindowPos(lblL, nullptr, leftX,                   py + 6,
                     colW - kEditW - 14, kLabelH, SWP_NOZORDER);
        SetWindowPos(edL,  nullptr, leftX + colW - kEditW,   py,
                     kEditW, kEditH, SWP_NOZORDER);
        SetWindowPos(lblR, nullptr, rightX,                  py + 6,
                     colW - kEditW - 14, kLabelH, SWP_NOZORDER);
        SetWindowPos(edR,  nullptr, rightX + colW - kEditW,  py,
                     kEditW, kEditH, SWP_NOZORDER);
        py += kRowH;
    };
    PlaceRow(hLblTgt_[0], hEditTargetMin_,  hLblDev_[0], hEditDevMin_);
    PlaceRow(hLblTgt_[1], hEditTargetMax_,  hLblDev_[1], hEditDevMax_);
    PlaceRow(hLblTgt_[2], hEditTargetVolt_, hLblDev_[2], hEditDevVolt_);
    y += kCardGap;

    // 4. ПОВЕДЕНИЕ — 2 ряда полей + 2 ряда чекбоксов
    const int behH = kRowH * 2 + 28 * 2 + 2 * kCardPad + 4;
    RECT c3 = Section(2, behH);
    int bx = c3.left + kCardPad;
    int by = c3.top + kCardPad;
    SetWindowPos(hLblPct_,          nullptr, bx,                 by + 6, colW - kEditW - 14, kLabelH, SWP_NOZORDER);
    SetWindowPos(hEditPercent_,     nullptr, bx + colW - kEditW, by,     kEditW, kEditH,              SWP_NOZORDER);
    SetWindowPos(hLblStartup_,      nullptr, rightX,             by + 6, colW - kEditW - 14, kLabelH, SWP_NOZORDER);
    SetWindowPos(hEditStartupDelay_,nullptr, rightX + colW - kEditW, by, kEditW, kEditH,              SWP_NOZORDER);
    by += kRowH;
    SetWindowPos(hLblPoll_,         nullptr, bx,                 by + 6, colW - kEditW - 14, kLabelH, SWP_NOZORDER);
    SetWindowPos(hEditPoll_,        nullptr, bx + colW - kEditW, by,     kEditW, kEditH,              SWP_NOZORDER);
    by += kRowH + 8;
    SetWindowPos(hChkAuto_,    nullptr, bx,     by, colW, 24, SWP_NOZORDER);
    SetWindowPos(hChkAlert_,   nullptr, rightX, by, colW, 24, SWP_NOZORDER);
    by += 28;
    SetWindowPos(hChkStartup_, nullptr, bx,     by, colW, 24, SWP_NOZORDER);
    SetWindowPos(hChkTray_,    nullptr, rightX, by, colW, 24, SWP_NOZORDER);
    y += kCardGap;

    // 5. ДЕЙСТВИЯ — 5 кнопок в ряд (без явной карточки, минимализм)
    SetWindowPos(hSec_[3], nullptr, x + 4, y, cx - 8, kSectionH, SWP_NOZORDER);
    y += kSectionH + kSectionGap;
    int gap = 10;
    int btnW = (cx - 4 * gap) / 5;
    HWND actBtns[5] = { hBtnCapture_, hBtnApply_, hBtnSave_, hBtnReload_, hBtnTest_ };
    for (int i = 0; i < 5; ++i) {
        SetWindowPos(actBtns[i], nullptr,
                     x + (btnW + gap) * i, y, btnW, kBtnH, SWP_NOZORDER);
    }
    y += kBtnH + kCardGap + 4;

    // 6. КАК ПОЛЬЗОВАТЬСЯ — карточка, занимает оставшееся место
    const int bottomRowH = kBtnH + kPad;
    int helpCardH = CH - y - kSectionH - kSectionGap - bottomRowH - kPad;
    if (helpCardH < 120) helpCardH = 120;
    RECT c5 = Section(4, helpCardH);
    SetWindowPos(hHelp_, nullptr,
                 c5.left + kCardPad, c5.top + kCardPad,
                 (c5.right - c5.left) - 2 * kCardPad,
                 helpCardH - 2 * kCardPad, SWP_NOZORDER);

    // Нижняя строка: Диагностика | Свернуть в трей
    int by2 = CH - kBtnH - kPad;
    int wide = (cx - 10) / 2;
    SetWindowPos(hBtnDiag_, nullptr, x,             by2, wide, kBtnH, SWP_NOZORDER);
    SetWindowPos(hBtnMin_,  nullptr, x + wide + 10, by2, wide, kBtnH, SWP_NOZORDER);

    InvalidateRect(hwnd_, nullptr, TRUE);
}

void UiMainWindow::LoadFromConfig() {
    SetEditInt(hEditTargetMin_,    cfg_.targetMinFreqMHz);
    SetEditInt(hEditTargetMax_,    cfg_.targetMaxFreqMHz);
    SetEditInt(hEditTargetVolt_,   cfg_.targetVoltageMv);
    SetEditInt(hEditDevMin_,       cfg_.allowedMinFreqDeviationMHz);
    SetEditInt(hEditDevMax_,       cfg_.allowedMaxFreqDeviationMHz);
    SetEditInt(hEditDevVolt_,      cfg_.allowedVoltageDeviationMv);
    SetEditInt(hEditPercent_,      cfg_.slippedPercentThreshold);
    SetEditInt(hEditStartupDelay_, cfg_.startupDelaySeconds);
    SetEditInt(hEditPoll_,         cfg_.pollIntervalSeconds);
    SendMessageW(hChkAuto_,    BM_SETCHECK, cfg_.autoReapplyEnabled     ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hChkAlert_,   BM_SETCHECK, cfg_.fullscreenAlertEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hChkStartup_, BM_SETCHECK, cfg_.startWithWindows       ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hChkTray_,    BM_SETCHECK, cfg_.minimizeToTray         ? BST_CHECKED : BST_UNCHECKED, 0);
}

void UiMainWindow::StoreToConfig() {
    cfg_.targetMinFreqMHz           = GetEditInt(hEditTargetMin_);
    cfg_.targetMaxFreqMHz           = GetEditInt(hEditTargetMax_);
    cfg_.targetVoltageMv            = GetEditInt(hEditTargetVolt_);
    cfg_.allowedMinFreqDeviationMHz = GetEditInt(hEditDevMin_);
    cfg_.allowedMaxFreqDeviationMHz = GetEditInt(hEditDevMax_);
    cfg_.allowedVoltageDeviationMv  = GetEditInt(hEditDevVolt_);
    cfg_.slippedPercentThreshold    = GetEditInt(hEditPercent_);
    cfg_.startupDelaySeconds        = GetEditInt(hEditStartupDelay_);
    cfg_.pollIntervalSeconds        = GetEditInt(hEditPoll_);
    cfg_.autoReapplyEnabled     = SendMessageW(hChkAuto_,    BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_.fullscreenAlertEnabled = SendMessageW(hChkAlert_,   BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_.startWithWindows       = SendMessageW(hChkStartup_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_.minimizeToTray         = SendMessageW(hChkTray_,    BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_.Clamp();
}

void UiMainWindow::RefreshStatusText(const EngineSnapshot& snap) {
    // Баннер: цвет задаётся через WM_ERASEBKGND (мы его не красим сами,
    // используем WM_CTLCOLORSTATIC, но для градиента паинт карточки
    // рисуется поверх в WM_PAINT). Здесь просто обновляем текст
    // и запоминаем текущее состояние для WM_PAINT.
    SetWindowTextW(hBanner_, L""); // сам текст рисуется в WM_PAINT
    InvalidateRect(hBanner_, nullptr, TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
    (void)snap;

    std::wstringstream w;
    w << L"ADLX:    " << (snap.adlxConnected ? L"подключено" : L"не подключено") << L"\r\n";
    if (snap.featureUnsupported) {
        w << L"GPU не поддерживает Manual GFX Tuning через ADLX.\r\n";
    } else {
        w << L"Текущие:    " << snap.last.minFreqMHz << L" / "
          << snap.last.maxFreqMHz << L" МГц     " << snap.last.voltageMv << L" мВ\r\n"
          << L"Целевые:    " << cfg_.targetMinFreqMHz << L" / "
          << cfg_.targetMaxFreqMHz << L" МГц     " << cfg_.targetVoltageMv << L" мВ\r\n"
          << L"Диапазон напряжения:    " << snap.last.rangeVoltMin << L" – "
          << snap.last.rangeVoltMax << L" мВ    (внутри: "
          << snap.voltagePercentInRange << L"%)\r\n"
          << L"Строгое несоответствие:    " << (snap.strictMismatch ? L"да" : L"нет")
          << L"       эвристический слип:    " << (snap.heuristicSlip ? L"да" : L"нет") << L"\r\n";
    }
    w << L"Последняя проверка:    " << snap.lastCheckTime << L"\r\n"
      << L"Последнее применение:  " << snap.lastReapplyResult;
    SetWindowTextW(hStatus_, w.str().c_str());
}

void UiMainWindow::OnEngineUpdate(const EngineSnapshot& snap) {
    RefreshStatusText(snap);
    MaybeShowAlertFromState(snap);
}

void UiMainWindow::MaybeShowAlertFromState(const EngineSnapshot& snap) {
    if (snap.state == EngineState::FailedToRecover && cfg_.fullscreenAlertEnabled) {
        if (!alert_.IsVisible()) {
            alert_.Show(hInst_, hwnd_,
                L"Андервольт AMD слетел и не был восстановлен.\n"
                L"Откройте AMD Software и примените свой профиль настройки заново.");
        }
    } else if (snap.state == EngineState::Healthy && alert_.IsVisible()) {
        alert_.Hide();
    }
}

void UiMainWindow::StartStartupDelayTimer() {
    SetTimer(hwnd_, kTimerStartupDelay, (UINT)cfg_.startupDelaySeconds * 1000U, nullptr);
}
void UiMainWindow::StartAdlxRetryTimer() {
    SetTimer(hwnd_, kTimerAdlxRetry, kAdlxRetryIntervalSec * 1000U, nullptr);
}
void UiMainWindow::StartPollTimer() {
    SetTimer(hwnd_, kTimerPoll, (UINT)cfg_.pollIntervalSeconds * 1000U, nullptr);
}
void UiMainWindow::KillAllTimers() {
    KillTimer(hwnd_, kTimerStartupDelay);
    KillTimer(hwnd_, kTimerAdlxRetry);
    KillTimer(hwnd_, kTimerPoll);
}

void UiMainWindow::AddTrayIcon() {
    if (trayAdded_) return;
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd   = hwnd_;
    nid_.uID    = TRAY_UID;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_APP_TRAY;
    nid_.hIcon  = (HICON)LoadImageW(hInst_, MAKEINTRESOURCEW(IDI_APPICON),
                                    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!nid_.hIcon) nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    StringCchCopyW(nid_.szTip, ARRAYSIZE(nid_.szTip), L"AMDUVGuard — страж андервольта");
    Shell_NotifyIconW(NIM_ADD, &nid_);
    trayAdded_ = true;
}

void UiMainWindow::RemoveTrayIcon() {
    if (!trayAdded_) return;
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    trayAdded_ = false;
}

void UiMainWindow::ShowFromTray() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}
void UiMainWindow::MinimizeToTray() {
    ShowWindow(hwnd_, SW_HIDE);
}

void UiMainWindow::HandleTrayMessage(LPARAM lParam) {
    if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
        ShowFromTray();
    } else if (lParam == WM_RBUTTONUP) {
        POINT pt; GetCursorPos(&pt);
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"Открыть");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"Выход");
        SetForegroundWindow(hwnd_);
        TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
        DestroyMenu(m);
    }
}

void UiMainWindow::DoSave() {
    StoreToConfig();
    cfg_.Save(Config::DefaultPath());
    Logger::Instance().SetLevel(cfg_.logLevel);
    KillTimer(hwnd_, kTimerPoll);
    StartPollTimer();
    if (cfg_.startWithWindows) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        SchedulerIntegration::Install(exe, cfg_.startupDelaySeconds);
    } else {
        SchedulerIntegration::Uninstall();
    }
    MessageBoxW(hwnd_, L"Настройки сохранены.", kAppName, MB_ICONINFORMATION);
}
void UiMainWindow::DoReload() {
    cfg_ = Config::LoadOrCreate(Config::DefaultPath());
    LoadFromConfig();
}
void UiMainWindow::DoApplyNow() {
    StoreToConfig();
    engine_.ApplyNow();
}
void UiMainWindow::DoCaptureCurrent() {
    if (engine_.CaptureAsTarget()) {
        LoadFromConfig();
        MessageBoxW(hwnd_,
            L"Поля заполнены текущими значениями из ADLX.\n"
            L"Не забудьте нажать «Сохранить».",
            kAppName, MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd_,
            L"Не удалось прочитать значения из ADLX.\n"
            L"Подождите, пока ADLX подключится, и попробуйте снова.",
            kAppName, MB_ICONWARNING);
    }
}
void UiMainWindow::DoTestWarning() {
    alert_.Show(hInst_, hwnd_,
        L"Это тестовое предупреждение — так выглядит тревога,\n"
        L"когда андервольт слетел и не был восстановлен.");
}
void UiMainWindow::DoExportDiagnostics() {
    OPENFILENAMEW ofn{}; wchar_t path[MAX_PATH] = L"amduvguard_diag.txt";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd_;
    ofn.lpstrFilter = L"Текстовый файл\0*.txt\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        Diagnostics::ExportToFile(path, adlx_, engine_.Snapshot(), cfg_);
    }
}

LRESULT UiMainWindow::Proc(UINT m, WPARAM w, LPARAM l) {
    switch (m) {

    case WM_ERASEBKGND: {
        HDC dc = (HDC)w;
        RECT rc; GetClientRect(hwnd_, &rc);
        FillRect(dc, &rc, hBrBg_);
        return 1;
    }

    // ── Карточки и градиентный баннер рисуются здесь ────────────────
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd_, &ps);

        // Буфер: чтобы не было мерцания, рисуем в memory DC.
        RECT clientRc; GetClientRect(hwnd_, &clientRc);
        HDC memDC = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, clientRc.right, clientRc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);
        FillRect(memDC, &clientRc, hBrBg_);

        {
            Gdiplus::Graphics g(memDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            // ── Баннер с градиентом ──
            RECT br; GetWindowRect(hBanner_, &br);
            MapWindowPoints(nullptr, hwnd_, (POINT*)&br, 2);
            Gdiplus::Rect bannerRect(br.left, br.top,
                                     br.right - br.left,
                                     br.bottom - br.top);

            COLORREF c1, c2; std::wstring bannerTitle;
            StateColors(engine_.Snapshot().state, c1, c2, bannerTitle);

            // Тень под баннером
            DrawSoftShadow(g, bannerRect, kBannerRadius);

            auto* bPath = RoundedPath(bannerRect, kBannerRadius);
            Gdiplus::LinearGradientBrush bg(
                Gdiplus::Point(bannerRect.X, bannerRect.Y),
                Gdiplus::Point(bannerRect.X + bannerRect.Width,
                               bannerRect.Y + bannerRect.Height),
                FromCOLORREF(c1), FromCOLORREF(c2));
            g.FillPath(&bg, bPath);
            delete bPath;

            // Текст заголовка (поверх градиента)
            Gdiplus::FontFamily ff(L"Segoe UI Semibold");
            Gdiplus::Font        f (&ff, 26, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::StringFormat sf;
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::SolidBrush white(FromCOLORREF(pal::BannerText));
            Gdiplus::RectF textRect((Gdiplus::REAL)bannerRect.X,
                                    (Gdiplus::REAL)bannerRect.Y,
                                    (Gdiplus::REAL)bannerRect.Width,
                                    (Gdiplus::REAL)bannerRect.Height);
            g.DrawString(bannerTitle.c_str(), -1, &f, textRect, &sf, &white);

            // ── Карточки ──
            for (const RECT& r : cardRects_) {
                Gdiplus::Rect gr(r.left, r.top, r.right - r.left, r.bottom - r.top);
                DrawSoftShadow(g, gr, kCardRadius);

                auto* path = RoundedPath(gr, kCardRadius);
                Gdiplus::SolidBrush fill(FromCOLORREF(pal::Card));
                g.FillPath(&fill, path);

                Gdiplus::Pen border(FromCOLORREF(pal::CardBorder), 1.0f);
                g.DrawPath(&border, path);
                delete path;
            }

            // ── Рамки вокруг полей ввода ──
            HWND edits[] = {
                hEditTargetMin_, hEditTargetMax_, hEditTargetVolt_,
                hEditDevMin_,    hEditDevMax_,    hEditDevVolt_,
                hEditPercent_,   hEditStartupDelay_, hEditPoll_
            };
            HWND focus = GetFocus();
            for (HWND e : edits) {
                if (!e) continue;
                RECT er; GetWindowRect(e, &er);
                MapWindowPoints(nullptr, hwnd_, (POINT*)&er, 2);
                InflateRect(&er, 2, 2);
                Gdiplus::Rect eg(er.left, er.top,
                                 er.right - er.left, er.bottom - er.top);
                auto* path = RoundedPath(eg, kEditRadius);
                Gdiplus::SolidBrush fillE(FromCOLORREF(pal::EditBg));
                g.FillPath(&fillE, path);
                bool focused = (e == focus);
                Gdiplus::Pen pen(FromCOLORREF(focused ? pal::EditFocus : pal::EditBorder),
                                 focused ? 1.5f : 1.0f);
                g.DrawPath(&pen, path);
                delete path;
            }
        }

        BitBlt(dc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        HWND hCtl = (HWND)l;
        SetBkMode(dc, TRANSPARENT);
        if (hCtl == hBanner_) {
            // Баннер рисуется в WM_PAINT, текст статического контрола
            // делаем невидимым (прозрачный + белый).
            SetTextColor(dc, pal::BannerText);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        HFONT f = (HFONT)SendMessageW(hCtl, WM_GETFONT, 0, 0);
        if (f == hFontSection_) {
            SetTextColor(dc, pal::Section);
            return (LRESULT)hBrBg_;
        }
        if (hCtl == hStatus_) {
            SetTextColor(dc, pal::Muted);
            return (LRESULT)hBrCard_;
        }
        if (hCtl == hHelp_) {
            SetTextColor(dc, pal::Muted);
            return (LRESULT)hBrCard_;
        }
        // Обычные подписи в карточках
        SetTextColor(dc, pal::Muted);
        return (LRESULT)hBrCard_;
    }
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)w;
        SetBkColor(dc, pal::Card);
        SetTextColor(dc, pal::Text);
        return (LRESULT)hBrCard_;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetBkColor(dc, pal::EditBg);
        SetTextColor(dc, pal::Text);
        return (LRESULT)hBrEdit_;
    }

    // ── Owner-draw кнопок ───────────────────────────────────────────
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)l;
        if (di->CtlType != ODT_BUTTON) break;

        bool hover   = GetPropW(di->hwndItem, L"hov") != nullptr;
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        int  id      = GetDlgCtrlID(di->hwndItem);
        bool primary = (id == IDC_BTN_PRIMARY);

        COLORREF bg, fg, border;
        if (primary) {
            bg = pressed ? pal::AccentPress
                         : (hover ? pal::AccentHover : pal::Accent);
            fg = pal::AccentText;
            border = bg;
        } else {
            bg = pressed ? pal::BtnBgPress
                         : (hover ? pal::BtnBgHover : pal::BtnBg);
            fg = pal::BtnText;
            border = pal::BtnBorder;
        }

        // Подложка — такая же, как у родителя (фон окна)
        HBRUSH bgBrush = CreateSolidBrush(pal::Bg);
        FillRect(di->hDC, &di->rcItem, bgBrush);
        DeleteObject(bgBrush);

        Gdiplus::Graphics g(di->hDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        Gdiplus::Rect gr(di->rcItem.left,  di->rcItem.top,
                         di->rcItem.right - di->rcItem.left - 1,
                         di->rcItem.bottom - di->rcItem.top - 1);
        auto* path = RoundedPath(gr, kBtnRadius);
        Gdiplus::SolidBrush fill(FromCOLORREF(bg));
        g.FillPath(&fill, path);
        Gdiplus::Pen pen(FromCOLORREF(border), 1.0f);
        g.DrawPath(&pen, path);
        delete path;

        wchar_t buf[128];
        GetWindowTextW(di->hwndItem, buf, 128);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, fg);
        HFONT old = (HFONT)SelectObject(di->hDC, hFontUi_);
        DrawTextW(di->hDC, buf, -1, &di->rcItem,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(di->hDC, old);
        return TRUE;
    }

    case WM_SIZE:
        DoLayout();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)l;
        mmi->ptMinTrackSize.x = kMinW;
        mmi->ptMinTrackSize.y = kMinH;
        return 0;
    }

    case WM_TIMER:
        if (w == kTimerStartupDelay) {
            KillTimer(hwnd_, kTimerStartupDelay);
            engine_.OnStartupDelayElapsed();
            if (adlx_.IsInitialized()) {
                adlx_.SubscribeTuningEvents(hwnd_);
                StartPollTimer();
            } else {
                StartAdlxRetryTimer();
            }
        } else if (w == kTimerAdlxRetry) {
            engine_.OnAdlxRetry();
            if (adlx_.IsInitialized()) {
                KillTimer(hwnd_, kTimerAdlxRetry);
                adlx_.SubscribeTuningEvents(hwnd_);
                StartPollTimer();
            }
        } else if (w == kTimerPoll) {
            engine_.OnPollTick();
        }
        return 0;

    case WM_APP_ADLX_EVENT:
        engine_.OnAdlxEvent();
        return 0;

    case WM_APP_TRAY:
        HandleTrayMessage(l);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(w);
        switch (id) {
        case IDC_BTN_SAVE:     DoSave();              break;
        case IDC_BTN_RELOAD:   DoReload();            break;
        case IDC_BTN_APPLY:    DoApplyNow();          break;
        case IDC_BTN_CAPTURE:  DoCaptureCurrent();    break;
        case IDC_BTN_TESTWARN: DoTestWarning();       break;
        case IDC_BTN_DIAG:     DoExportDiagnostics(); break;
        case IDC_BTN_MIN:      MinimizeToTray();      break;
        case IDM_TRAY_SHOW:    ShowFromTray();        break;
        case IDM_TRAY_EXIT:    quitting_ = true; DestroyWindow(hwnd_); break;
        case AlertWindow::kIdRetry:    DoApplyNow();   break;
        case AlertWindow::kIdSettings: ShowFromTray(); alert_.Hide(); break;
        case AlertWindow::kIdDismiss:  alert_.Hide();  break;
        }
        return 0;
    }

    case WM_SYSCOMMAND:
        if ((w & 0xFFF0) == SC_MINIMIZE && cfg_.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        break;

    case WM_CLOSE:
        if (cfg_.minimizeToTray && !quitting_) {
            MinimizeToTray();
            return 0;
        }
        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        KillAllTimers();
        adlx_.UnsubscribeTuningEvents();
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

} // namespace uvg
