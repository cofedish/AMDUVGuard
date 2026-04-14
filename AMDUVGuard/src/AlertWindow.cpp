#include "AlertWindow.h"
#include <commctrl.h>

namespace uvg {

namespace {
bool g_registered = false;
} // namespace

void AlertWindow::Register(HINSTANCE hInst) {
    if (g_registered) return;
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc   = Proc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kAlertWndClass;
    RegisterClassExW(&wc);
    g_registered = true;
}

bool AlertWindow::Show(HINSTANCE hInst, HWND owner, const std::wstring& message) {
    Register(hInst);
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd_);
        SetWindowTextW(GetDlgItem(hwnd_, 1), message.c_str());
        return true;
    }

    const int W = 640, H = 260;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                            kAlertWndClass,
                            L"AMDUVGuard — андервольт слетел",
                            WS_POPUP | WS_BORDER | WS_VISIBLE,
                            (sx - W) / 2, (sy - H) / 2, W, H,
                            owner, nullptr, hInst, this);
    if (!hwnd_) return false;

    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(11, 96, 72);
    lf.lfWeight = FW_SEMIBOLD;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW(lf.lfFaceName, L"Segoe UI");
    HFONT font = CreateFontIndirectW(&lf);
    auto Mk = [&](DWORD style, int id, const wchar_t* t, int x, int y, int w, int h) {
        HWND c = CreateWindowExW(0, L"BUTTON", t,
                                 WS_CHILD | WS_VISIBLE | style,
                                 x, y, w, h, hwnd_, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };
    HWND lbl = CreateWindowExW(0, L"STATIC", message.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 30, W - 40, 110, hwnd_, (HMENU)1, hInst, nullptr);
    SendMessageW(lbl, WM_SETFONT, (WPARAM)font, TRUE);

    Mk(BS_PUSHBUTTON, kIdRetry,    L"Применить заново",  60,  170, 170, 38);
    Mk(BS_PUSHBUTTON, kIdSettings, L"Открыть настройки", 245, 170, 170, 38);
    Mk(BS_PUSHBUTTON, kIdDismiss,  L"Закрыть",           430, 170, 150, 38);

    SetForegroundWindow(hwnd_);
    return true;
}

void AlertWindow::Hide() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK AlertWindow::Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND) {
        // Forward command to owner so AppMainWindow can react.
        HWND owner = GetWindow(h, GW_OWNER);
        if (owner) PostMessageW(owner, WM_COMMAND, w, l);
        if (LOWORD(w) == kIdDismiss) {
            ShowWindow(h, SW_HIDE);
        }
        return 0;
    }
    if (m == WM_CLOSE) {
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace uvg
