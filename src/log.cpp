#include "log.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

static FILE* g_file = nullptr;
static std::mutex g_lock;
static bool g_debug = false;

void LogOpen()
{
    PWSTR docs = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)))
        path = std::wstring(docs) + L"\\My Games\\Skyrim VR\\SKSE";
    CoTaskMemFree(docs);
    if (path.empty()) return; // no Documents folder: run without a log
    SHCreateDirectoryExW(nullptr, path.c_str(), nullptr); // usually exists already
    path += L"\\FrameKeyboardSummoner.log";
    _wfopen_s(&g_file, path.c_str(), L"w");
}

void LogSetDebug(bool on) { g_debug = on; }

static void Write(const char* fmt, va_list args)
{
    if (!g_file) return;
    std::lock_guard<std::mutex> guard(g_lock);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    vfprintf(g_file, fmt, args);
    fputc('\n', g_file);
    fflush(g_file); // keep the log complete if the game crashes
}

void Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Write(fmt, args);
    va_end(args);
}

void LogDebug(const char* fmt, ...)
{
    if (!g_debug) return;
    va_list args;
    va_start(args, fmt);
    Write(fmt, args);
    va_end(args);
}
