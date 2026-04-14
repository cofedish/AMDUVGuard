// AMDUVGuard - common constants & helpers
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <string_view>
#include <cstdint>

namespace uvg {

// Application identity
inline constexpr wchar_t kAppName[]        = L"AMDUVGuard";
inline constexpr wchar_t kAppVersion[]     = L"1.0.0";
inline constexpr wchar_t kMutexName[]      = L"Local\\AMDUVGuard.SingleInstance.A1B2";
inline constexpr wchar_t kMainWndClass[]   = L"AMDUVGuard.Main";
inline constexpr wchar_t kAlertWndClass[]  = L"AMDUVGuard.Alert";
inline constexpr wchar_t kTrayTipText[]    = L"AMDUVGuard - undervolt watchdog";
inline constexpr wchar_t kTaskName[]       = L"AMDUVGuard Autostart";

// Defaults
inline constexpr int kDefaultStartupDelaySec = 90;
inline constexpr int kDefaultPollIntervalSec = 10;
inline constexpr int kMinPollIntervalSec     = 5;
inline constexpr int kReapplyCooldownSec     = 60;
inline constexpr int kAdlxRetryIntervalSec   = 20;
inline constexpr int kDebounceConfirmCount   = 2;
inline constexpr int kDefaultSlipPercent     = 98;

// Custom WM messages (UI thread)
inline constexpr UINT WM_APP_TRAY            = WM_APP + 1;
inline constexpr UINT WM_APP_ADLX_EVENT      = WM_APP + 2;
inline constexpr UINT WM_APP_TICK            = WM_APP + 3;
inline constexpr UINT WM_APP_SHOW_ALERT      = WM_APP + 4;

// Timer ids
inline constexpr UINT_PTR kTimerStartupDelay = 1001;
inline constexpr UINT_PTR kTimerAdlxRetry    = 1002;
inline constexpr UINT_PTR kTimerPoll         = 1003;

std::wstring Utf8ToWide(std::string_view utf8);
std::string  WideToUtf8(std::wstring_view w);
std::wstring GetExeDir();
std::wstring GetAppDataDir();    // %AppData%\AMDUVGuard, ensures exists
std::wstring NowTimestamp();     // YYYY-MM-DD HH:MM:SS

} // namespace uvg
