#include "SchedulerIntegration.h"
#include "Logging.h"
#include <fstream>
#include <sstream>
#include <vector>

namespace uvg {

namespace {
std::wstring CurrentUser() {
    wchar_t buf[256]; DWORD n = 256;
    if (GetUserNameW(buf, &n)) return buf;
    return L"";
}

std::wstring TempXmlPath() {
    wchar_t buf[MAX_PATH]; GetTempPathW(MAX_PATH, buf);
    return std::wstring(buf) + L"amduvguard_task.xml";
}

bool RunSchtasks(const std::wstring& args) {
    std::wstring cmd = L"schtasks.exe " + args;
    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(0);
    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}
} // namespace

namespace {
// Экранирование XML-спецсимволов в пользовательских строках.
std::wstring XmlEscape(const std::wstring& s) {
    std::wstring r; r.reserve(s.size());
    for (wchar_t c : s) {
        switch (c) {
            case L'&':  r += L"&amp;";  break;
            case L'<':  r += L"&lt;";   break;
            case L'>':  r += L"&gt;";   break;
            case L'"':  r += L"&quot;"; break;
            case L'\'': r += L"&apos;"; break;
            default:    r += c;         break;
        }
    }
    return r;
}

// Пишет буфер wchar_t в файл как UTF-16 LE с BOM — именно это
// требует schtasks /XML. std::wofstream так не умеет.
bool WriteUtf16Le(const std::wstring& path, const std::wstring& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    // BOM
    const unsigned char bom[2] = { 0xFF, 0xFE };
    DWORD wr = 0;
    WriteFile(h, bom, 2, &wr, nullptr);
    WriteFile(h, data.data(),
              (DWORD)(data.size() * sizeof(wchar_t)), &wr, nullptr);
    CloseHandle(h);
    return true;
}
} // namespace

bool SchedulerIntegration::Install(const std::wstring& exePath, int delaySec) {
    std::wstring user = CurrentUser();
    std::wstring xml  = TempXmlPath();

    std::wstring userX = XmlEscape(user);
    std::wstring exeX  = XmlEscape(exePath);

    std::wstringstream ss;
    ss << L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
       << L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
       << L"  <RegistrationInfo>\r\n"
       << L"    <Author>" << userX << L"</Author>\r\n"
       << L"    <Description>AMDUVGuard autostart</Description>\r\n"
       << L"  </RegistrationInfo>\r\n"
       << L"  <Triggers>\r\n"
       << L"    <LogonTrigger>\r\n"
       << L"      <Enabled>true</Enabled>\r\n"
       << L"      <Delay>PT" << delaySec << L"S</Delay>\r\n"
       << L"      <UserId>" << userX << L"</UserId>\r\n"
       << L"    </LogonTrigger>\r\n"
       << L"  </Triggers>\r\n"
       << L"  <Principals>\r\n"
       << L"    <Principal id=\"Author\">\r\n"
       << L"      <UserId>" << userX << L"</UserId>\r\n"
       << L"      <LogonType>InteractiveToken</LogonType>\r\n"
       << L"      <RunLevel>LeastPrivilege</RunLevel>\r\n"
       << L"    </Principal>\r\n"
       << L"  </Principals>\r\n"
       << L"  <Settings>\r\n"
       << L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\r\n"
       << L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\r\n"
       << L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\r\n"
       << L"    <AllowHardTerminate>true</AllowHardTerminate>\r\n"
       << L"    <StartWhenAvailable>true</StartWhenAvailable>\r\n"
       << L"    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\r\n"
       << L"    <IdleSettings>\r\n"
       << L"      <StopOnIdleEnd>false</StopOnIdleEnd>\r\n"
       << L"      <RestartOnIdle>false</RestartOnIdle>\r\n"
       << L"    </IdleSettings>\r\n"
       << L"    <AllowStartOnDemand>true</AllowStartOnDemand>\r\n"
       << L"    <Enabled>true</Enabled>\r\n"
       << L"    <Hidden>false</Hidden>\r\n"
       << L"    <RunOnlyIfIdle>false</RunOnlyIfIdle>\r\n"
       << L"    <WakeToRun>false</WakeToRun>\r\n"
       << L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\r\n"
       << L"    <Priority>7</Priority>\r\n"
       << L"  </Settings>\r\n"
       << L"  <Actions Context=\"Author\">\r\n"
       << L"    <Exec>\r\n"
       << L"      <Command>" << exeX << L"</Command>\r\n"
       << L"    </Exec>\r\n"
       << L"  </Actions>\r\n"
       << L"</Task>\r\n";

    if (!WriteUtf16Le(xml, ss.str())) {
        Logger::Instance().Error(L"Scheduler: cannot write XML to " + xml);
        return false;
    }
    Logger::Instance().Info(L"Scheduler: XML written to " + xml);
    Logger::Instance().Info(L"Scheduler: exe=" + exePath);

    bool ok = RunSchtasks(L"/Create /F /TN \"" + std::wstring(kTaskName) +
                          L"\" /XML \"" + xml + L"\"");
    if (!ok) {
        Logger::Instance().Error(L"Scheduler: schtasks /Create failed");
        // Не удаляем XML — оставим для диагностики
    } else {
        DeleteFileW(xml.c_str());
        Logger::Instance().Info(L"Scheduler: task installed");
    }
    return ok;
}

bool SchedulerIntegration::Uninstall() {
    bool ok = RunSchtasks(L"/Delete /F /TN \"" + std::wstring(kTaskName) + L"\"");
    Logger::Instance().Info(ok ? L"Scheduler: task removed"
                               : L"Scheduler: remove failed");
    return ok;
}

bool SchedulerIntegration::IsInstalled() {
    return RunSchtasks(L"/Query /TN \"" + std::wstring(kTaskName) + L"\"");
}

} // namespace uvg
