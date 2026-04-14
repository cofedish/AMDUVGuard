#pragma once
#include "Common.h"
#include <mutex>
#include <string>

namespace uvg {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

// Thread-safe logger with size-based rotation (3 files x ~1MB).
// Coalesces identical consecutive messages to avoid log spam.
class Logger {
public:
    static Logger& Instance();

    void Init(const std::wstring& dir, LogLevel level);
    void SetLevel(LogLevel level);
    LogLevel GetLevel() const { return level_; }

    void Log(LogLevel lvl, const std::wstring& msg);

    // Convenience
    void Info (const std::wstring& m) { Log(LogLevel::Info,  m); }
    void Warn (const std::wstring& m) { Log(LogLevel::Warn,  m); }
    void Error(const std::wstring& m) { Log(LogLevel::Error, m); }
    void Debug(const std::wstring& m) { Log(LogLevel::Debug, m); }

    void Shutdown();

private:
    Logger() = default;
    void RotateIfNeeded_NoLock();

    std::mutex   mu_;
    std::wstring path_;
    std::wstring dir_;
    LogLevel     level_ = LogLevel::Info;
    HANDLE       file_  = INVALID_HANDLE_VALUE;
    std::wstring lastMsg_;
    int          lastRepeat_ = 0;
    LogLevel     lastLvl_    = LogLevel::Info;
};

} // namespace uvg
