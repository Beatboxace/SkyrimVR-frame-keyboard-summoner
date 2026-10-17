#include "config.h"
#include "log.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

Config g_cfg;

// Path of this DLL with ".dll" swapped for ".ini". Under MO2 the virtual file system maps
// Data\SKSE\Plugins onto the mod folder, so this finds the INI shipped with the mod.
static std::wstring IniPath()
{
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&IniPath), &self);
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(self, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    std::wstring path(buf, n);
    size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos) path.resize(dot);
    return path + L".ini";
}

static std::string Trim(const std::string& s)
{
    static const char kSpace[] = " \t\r\n"; // fgets keeps the line's newline
    size_t a = s.find_first_not_of(kSpace);
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(kSpace);
    return s.substr(a, b - a + 1);
}

static std::string Lower(std::string s)
{
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// The INI is parsed by hand on purpose: mods like PrivateProfileRedirector hook the Windows
// GetPrivateProfile* APIs and can serve cached values that don't match the file on disk.
// Result keys are "section.key", lowercase.
static std::map<std::string, std::string> ParseIni(const std::wstring& path)
{
    std::map<std::string, std::string> values;
    FILE* f = nullptr;
    if (path.empty() || _wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return values;
    std::string section;
    char line[512];
    for (bool first = true; fgets(line, sizeof(line), f); first = false) {
        std::string s = line;
        if (first && s.compare(0, 3, "\xEF\xBB\xBF") == 0) s.erase(0, 3); // UTF-8 BOM (Notepad)
        s = Trim(s);
        if (s.empty() || s[0] == ';' || s[0] == '#') continue;
        if (s[0] == '[') {
            size_t end = s.find(']');
            section = Lower(Trim(s.substr(1, end == std::string::npos ? std::string::npos : end - 1)));
            continue;
        }
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string value = s.substr(eq + 1);
        size_t comment = value.find_first_of(";#"); // allow "Key=1 ; comment"
        if (comment != std::string::npos) value.resize(comment);
        values[section + "." + Lower(Trim(s.substr(0, eq)))] = Trim(value);
    }
    fclose(f);
    return values;
}

using Values = std::map<std::string, std::string>;

static bool GetBool(const Values& v, const char* key, bool def)
{
    auto it = v.find(key);
    if (it == v.end() || it->second.empty()) return def;
    return atof(it->second.c_str()) != 0.0 || Lower(it->second) == "true";
}

static float GetFloat(const Values& v, const char* key, float def, float lo, float hi)
{
    auto it = v.find(key);
    if (it == v.end() || it->second.empty()) return def;
    float f = (float)atof(it->second.c_str());
    if (!(f >= lo)) return lo; // also catches "nan"
    return f > hi ? hi : f;
}

// A single letter or digit, e.g. "K". Anything else keeps the default.
static int GetKey(const Values& v, const char* key, int def)
{
    auto it = v.find(key);
    if (it == v.end()) return def;
    if (it->second.size() == 1) {
        int c = toupper((unsigned char)it->second[0]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
    }
    Log("config: %s=\"%s\" is not a single letter or digit, using %c", key, it->second.c_str(), def);
    return def;
}

void LoadConfig()
{
    std::wstring path = IniPath();
    Values v = ParseIni(path);
    if (v.empty()) Log("config: %ls not found or empty, using defaults", path.c_str());

    Config& c = g_cfg;
    c.debugLog = GetBool(v, "general.debuglog", c.debugLog);
    c.keyboardFix = GetBool(v, "keyboard.enabled", c.keyboardFix);
    c.overlay.enabled = GetBool(v, "textoverlay.enabled", c.overlay.enabled);
    c.overlay.followHead = GetBool(v, "textoverlay.followhead", c.overlay.followHead);
    c.overlay.width = GetFloat(v, "textoverlay.widthmeters", c.overlay.width, 0.1f, 3.0f);
    c.overlay.distance = GetFloat(v, "textoverlay.distancemeters", c.overlay.distance, 0.2f, 5.0f);
    c.overlay.offsetX = GetFloat(v, "textoverlay.offsetx", c.overlay.offsetX, -2.0f, 2.0f);
    c.overlay.offsetY = GetFloat(v, "textoverlay.offsety", c.overlay.offsetY, -2.0f, 2.0f);
    c.hotkeys.enabled = GetBool(v, "hotkeys.enabled", c.hotkeys.enabled);
    c.hotkeys.summonKey = GetKey(v, "hotkeys.summonkey", c.hotkeys.summonKey);
    c.hotkeys.hideKey = GetKey(v, "hotkeys.hidekey", c.hotkeys.hideKey);

    Log("config: DebugLog=%d Keyboard.Enabled=%d", c.debugLog, c.keyboardFix);
    Log("config: TextOverlay Enabled=%d FollowHead=%d Width=%.2f Distance=%.2f Offset=(%.2f, %.2f)",
        c.overlay.enabled, c.overlay.followHead, c.overlay.width, c.overlay.distance,
        c.overlay.offsetX, c.overlay.offsetY);
    Log("config: Hotkeys Enabled=%d Summon=Ctrl+Alt+%c Hide=Ctrl+Alt+%c",
        c.hotkeys.enabled, c.hotkeys.summonKey, c.hotkeys.hideKey);
}
