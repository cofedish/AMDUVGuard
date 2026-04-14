#pragma once
#include "Common.h"

namespace uvg {

// Borderless centered topmost warning window.
//
// Buttons emit IDs back to the owner via WM_COMMAND.
class AlertWindow {
public:
    static constexpr int kIdRetry    = 5101;
    static constexpr int kIdDismiss  = 5102;
    static constexpr int kIdSettings = 5103;

    bool Show(HINSTANCE hInst, HWND owner, const std::wstring& message);
    void Hide();
    bool IsVisible() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }

private:
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    static void Register(HINSTANCE);
    HWND hwnd_ = nullptr;
};

} // namespace uvg
