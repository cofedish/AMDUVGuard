#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

namespace uvg {

namespace {

// Tiny ad-hoc JSON writer/reader sufficient for a flat object of int/bool.
// We deliberately avoid pulling a full JSON dependency.

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

bool ParseInt(const std::string& v, int& out) {
    try { out = std::stoi(v); return true; } catch (...) { return false; }
}

void ParseFlat(const std::string& text,
               std::function<void(const std::string&, const std::string&)> cb) {
    // very small parser: looks for "key" : value entries
    size_t i = 0;
    while (i < text.size()) {
        size_t k1 = text.find('"', i);
        if (k1 == std::string::npos) break;
        size_t k2 = text.find('"', k1 + 1);
        if (k2 == std::string::npos) break;
        std::string key = text.substr(k1 + 1, k2 - k1 - 1);
        size_t colon = text.find(':', k2);
        if (colon == std::string::npos) break;
        size_t end = text.find_first_of(",}\n", colon + 1);
        if (end == std::string::npos) end = text.size();
        std::string val = Trim(text.substr(colon + 1, end - colon - 1));
        if (!val.empty() && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);
        cb(key, val);
        i = end + 1;
    }
}

} // namespace

void Config::Clamp() {
    targetMinFreqMHz           = std::clamp(targetMinFreqMHz, 100, 5000);
    targetMaxFreqMHz           = std::clamp(targetMaxFreqMHz, 100, 5000);
    if (targetMaxFreqMHz < targetMinFreqMHz) targetMaxFreqMHz = targetMinFreqMHz;
    targetVoltageMv            = std::clamp(targetVoltageMv, 600, 1300);
    allowedMinFreqDeviationMHz = std::clamp(allowedMinFreqDeviationMHz, 0, 500);
    allowedMaxFreqDeviationMHz = std::clamp(allowedMaxFreqDeviationMHz, 0, 500);
    allowedVoltageDeviationMv  = std::clamp(allowedVoltageDeviationMv, 0, 200);
    slippedPercentThreshold    = std::clamp(slippedPercentThreshold, 50, 100);
    startupDelaySeconds        = std::clamp(startupDelaySeconds, 0, 600);
    pollIntervalSeconds        = std::clamp(pollIntervalSeconds, kMinPollIntervalSec, 600);
}

std::wstring Config::DefaultPath() {
    return GetAppDataDir() + L"\\config.json";
}

Config Config::LoadOrCreate(const std::wstring& path) {
    Config c;
    std::ifstream f(path);
    if (!f.good()) {
        c.Save(path);
        return c;
    }
    std::stringstream ss; ss << f.rdbuf();
    std::string text = ss.str();
    ParseFlat(text, [&](const std::string& k, const std::string& v) {
        int iv = 0;
        if (k == "targetMinFreqMHz")            { if (ParseInt(v, iv)) c.targetMinFreqMHz = iv; }
        else if (k == "targetMaxFreqMHz")       { if (ParseInt(v, iv)) c.targetMaxFreqMHz = iv; }
        else if (k == "targetVoltageMv")        { if (ParseInt(v, iv)) c.targetVoltageMv = iv; }
        else if (k == "allowedMinFreqDeviationMHz") { if (ParseInt(v, iv)) c.allowedMinFreqDeviationMHz = iv; }
        else if (k == "allowedMaxFreqDeviationMHz") { if (ParseInt(v, iv)) c.allowedMaxFreqDeviationMHz = iv; }
        else if (k == "allowedVoltageDeviationMv")  { if (ParseInt(v, iv)) c.allowedVoltageDeviationMv = iv; }
        else if (k == "slippedPercentThreshold")    { if (ParseInt(v, iv)) c.slippedPercentThreshold = iv; }
        else if (k == "startupDelaySeconds")        { if (ParseInt(v, iv)) c.startupDelaySeconds = iv; }
        else if (k == "pollIntervalSeconds")        { if (ParseInt(v, iv)) c.pollIntervalSeconds = iv; }
        else if (k == "autoReapplyEnabled")         { c.autoReapplyEnabled = (v == "true" || v == "1"); }
        else if (k == "fullscreenAlertEnabled")     { c.fullscreenAlertEnabled = (v == "true" || v == "1"); }
        else if (k == "startWithWindows")           { c.startWithWindows = (v == "true" || v == "1"); }
        else if (k == "minimizeToTray")             { c.minimizeToTray = (v == "true" || v == "1"); }
        else if (k == "logLevel")                   { if (ParseInt(v, iv)) c.logLevel = static_cast<LogLevel>(iv); }
    });
    c.Clamp();
    return c;
}

bool Config::Save(const std::wstring& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.good()) return false;
    auto B = [](bool v){ return v ? "true" : "false"; };
    f << "{\n"
      << "  \"targetMinFreqMHz\": "            << targetMinFreqMHz << ",\n"
      << "  \"targetMaxFreqMHz\": "            << targetMaxFreqMHz << ",\n"
      << "  \"targetVoltageMv\": "             << targetVoltageMv << ",\n"
      << "  \"allowedMinFreqDeviationMHz\": "  << allowedMinFreqDeviationMHz << ",\n"
      << "  \"allowedMaxFreqDeviationMHz\": "  << allowedMaxFreqDeviationMHz << ",\n"
      << "  \"allowedVoltageDeviationMv\": "   << allowedVoltageDeviationMv << ",\n"
      << "  \"slippedPercentThreshold\": "     << slippedPercentThreshold << ",\n"
      << "  \"startupDelaySeconds\": "         << startupDelaySeconds << ",\n"
      << "  \"pollIntervalSeconds\": "         << pollIntervalSeconds << ",\n"
      << "  \"autoReapplyEnabled\": "          << B(autoReapplyEnabled) << ",\n"
      << "  \"fullscreenAlertEnabled\": "      << B(fullscreenAlertEnabled) << ",\n"
      << "  \"startWithWindows\": "            << B(startWithWindows) << ",\n"
      << "  \"minimizeToTray\": "              << B(minimizeToTray) << ",\n"
      << "  \"logLevel\": "                    << static_cast<int>(logLevel) << "\n"
      << "}\n";
    return true;
}

} // namespace uvg
