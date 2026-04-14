#include "Logging.h"
#include <shlobj.h>
#include <strsafe.h>

namespace uvg {

namespace {
constexpr DWORD kMaxFileBytes = 1 * 1024 * 1024;
constexpr int   kKeepFiles    = 3;

const wchar_t* LevelStr(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return L"TRC";
        case LogLevel::Debug: return L"DBG";
        case LogLevel::Info:  return L"INF";
        case LogLevel::Warn:  return L"WRN";
        case LogLevel::Error: return L"ERR";
    }
    return L"???";
}
} // namespace

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

void Logger::Init(const std::wstring& dir, LogLevel level) {
    std::lock_guard<std::mutex> lk(mu_);
    dir_   = dir;
    level_ = level;
    CreateDirectoryW(dir_.c_str(), nullptr);
    path_ = dir_ + L"\\amduvguard.log";
    file_ = CreateFileW(path_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(mu_);
    level_ = level;
}

void Logger::RotateIfNeeded_NoLock() {
    if (file_ == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(file_, &sz) || sz.QuadPart < kMaxFileBytes) return;
    CloseHandle(file_); file_ = INVALID_HANDLE_VALUE;
    for (int i = kKeepFiles - 1; i >= 1; --i) {
        std::wstring src = dir_ + L"\\amduvguard." + std::to_wstring(i) + L".log";
        std::wstring dst = dir_ + L"\\amduvguard." + std::to_wstring(i + 1) + L".log";
        MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    MoveFileExW(path_.c_str(), (dir_ + L"\\amduvguard.1.log").c_str(),
                MOVEFILE_REPLACE_EXISTING);
    file_ = CreateFileW(path_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Logger::Log(LogLevel lvl, const std::wstring& msg) {
    if (lvl < level_) return;
    std::lock_guard<std::mutex> lk(mu_);
    if (file_ == INVALID_HANDLE_VALUE) return;

    // Coalesce identical consecutive messages
    if (msg == lastMsg_ && lvl == lastLvl_) {
        ++lastRepeat_;
        if (lastRepeat_ < 100) return;
    }
    if (lastRepeat_ > 0) {
        wchar_t buf[128];
        StringCchPrintfW(buf, 128, L"   ... previous message repeated %d times\r\n",
                         lastRepeat_);
        DWORD wr = 0;
        std::string a = WideToUtf8(buf);
        WriteFile(file_, a.data(), (DWORD)a.size(), &wr, nullptr);
        lastRepeat_ = 0;
    }
    lastMsg_ = msg;
    lastLvl_ = lvl;

    RotateIfNeeded_NoLock();
    if (file_ == INVALID_HANDLE_VALUE) return;

    std::wstring line = NowTimestamp() + L" [" + LevelStr(lvl) + L"] " + msg + L"\r\n";
    std::string a = WideToUtf8(line);
    DWORD wr = 0;
    WriteFile(file_, a.data(), (DWORD)a.size(), &wr, nullptr);
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lk(mu_);
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

// ----- Common.h helpers live here for convenience -----

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n,
                        nullptr, nullptr);
    return s;
}

std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    auto pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring GetAppDataDir() {
    wchar_t* roaming = nullptr;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming);
    std::wstring d = roaming ? roaming : L".";
    if (roaming) CoTaskMemFree(roaming);
    d += L"\\AMDUVGuard";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

std::wstring NowTimestamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t buf[32];
    StringCchPrintfW(buf, 32, L"%04u-%02u-%02u %02u:%02u:%02u",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

} // namespace uvg
