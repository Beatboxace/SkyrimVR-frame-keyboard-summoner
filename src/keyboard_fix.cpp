#include "keyboard_fix.h"
#include "log.h"
#include "text_overlay.h"
#include "vr.h"

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

// ---- vtable patching ---------------------------------------------------------------------
// IVROverlay_018 slot numbers, counted from openvr.h at ValveSoftware/openvr 6aacebd
// (the header version that matches Skyrim VR's openvr_api.dll).
enum {
    kPollNextOverlayEvent = 45,
    kShowKeyboardForOverlay = 73,
    kGetKeyboardText = 74,
    kHideKeyboard = 75,
};

// Swap one vtable slot for our hook and keep the original. If the slot already holds our hook
// (OpenVR re-initialised and handed back the same vtable), leave it alone: saving our own hook as
// the "original" would make the hook call itself forever.
static bool PatchSlot(void* obj, int index, void* hook, void** orig)
{
    void** vtbl = *reinterpret_cast<void***>(obj);
    if (vtbl[index] == hook) return true;
    DWORD old;
    if (!VirtualProtect(&vtbl[index], sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = vtbl[index];
    vtbl[index] = hook;
    VirtualProtect(&vtbl[index], sizeof(void*), old, &old);
    return true;
}

using ShowKeyboardForOverlay_t = vr::EVROverlayError (*)(void*, vr::VROverlayHandle_t, vr::EGamepadTextInputMode,
    vr::EGamepadTextInputLineMode, const char*, uint32_t, const char*, bool, uint64_t);
using GetKeyboardText_t = uint32_t (*)(void*, char*, uint32_t);
using HideKeyboard_t = void (*)(void*);
using PollNextOverlayEvent_t = bool (*)(void*, vr::VROverlayHandle_t, vr::VREvent_t*, uint32_t);

static ShowKeyboardForOverlay_t g_origShowForOverlay;
static GetKeyboardText_t g_origGetText;
static HideKeyboard_t g_origHide;
static PollNextOverlayEvent_t g_origPoll;

// ---- state -------------------------------------------------------------------------------
// Everything below is guarded by g_lock: the game uses it from its main thread, the optional
// hotkeys from their own thread. Recursive because the hotkey path re-enters the hooks.
static std::recursive_mutex g_lock;
static vr::IVROverlay* g_overlay = nullptr; // the patched interface

// The text accumulator. The compact keyboard replaces its buffer on every keypress instead of
// appending, so GetKeyboardText only ever holds the last character. We read it after each
// KeyboardCharInput event, rebuild the full string and hand that to the game instead.
// mode: 0 = not known yet, 1 = buffer holds the full text (normal keyboard, used as is),
//       2 = buffer holds one key at a time (compact keyboard, we accumulate).
static struct {
    bool        active = false; // a keyboard we opened is on screen
    std::string text;
    int         mode = 0;
    int         keys = 0;
    size_t      maxBytes = 255; // the game's limit minus the terminator
} g_kb;

// Arguments from the game's last request, reused by the summon hotkey.
static struct {
    vr::EGamepadTextInputMode     inputMode = vr::k_EGamepadTextInputModeNormal;
    vr::EGamepadTextInputLineMode lineMode = vr::k_EGamepadTextInputLineModeSingleLine;
    std::string                   description;
    std::string                   existing;
    uint32_t                      maxChars = 64;
} g_last;

// Remove the last UTF-8 character (not just the last byte, which would break accented letters).
static void PopUtf8(std::string& s)
{
    while (!s.empty()) {
        unsigned char c = (unsigned char)s.back();
        s.pop_back();
        if ((c & 0xC0) != 0x80) break; // stop after the lead byte
    }
}

// Open the plain keyboard, the one that works on the Steam Frame, and start a fresh accumulator.
static vr::EVROverlayError OpenKeyboard(vr::IVROverlay* o, vr::EGamepadTextInputMode inputMode,
    vr::EGamepadTextInputLineMode lineMode, const char* desc, uint32_t maxChars, const char* existing,
    bool minimal, uint64_t userValue)
{
    g_kb.text.clear();
    g_kb.mode = 0;
    g_kb.keys = 0;
    g_kb.maxBytes = maxChars > 1 ? maxChars - 1 : 255;
    g_kb.active = true;

    // ShowKeyboard isn't patched, so this calls SteamVR directly.
    vr::EVROverlayError r = o->ShowKeyboard(inputMode, lineMode, desc, maxChars, existing, minimal, userValue);
    if (r == vr::VROverlayError_KeyboardAlreadyInUse) {
        // A previous keyboard is still registered (e.g. one that never closed): close it, retry once.
        g_origHide(o);
        r = o->ShowKeyboard(inputMode, lineMode, desc, maxChars, existing, minimal, userValue);
    }
    if (r == vr::VROverlayError_None) TextOverlayOpen(o);
    else g_kb.active = false;
    return r;
}

static void CloseKeyboardState(const char* why)
{
    if (g_kb.active) Log("keyboard %s (%zu bytes typed)", why, g_kb.text.size());
    g_kb.active = false;
    g_kb.text.clear();
    TextOverlayHide(g_overlay);
}

// A key was pressed on our keyboard: update the accumulator and redraw the text box.
static void OnKeyboardChar(void* self)
{
    char buf[256] = {};
    uint32_t n = g_origGetText(self, buf, sizeof(buf));
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    std::string key(buf, n);
    while (!key.empty() && key.back() == '\0') key.pop_back(); // some runtimes count the terminator
    ++g_kb.keys;

    // More than one character in the buffer means it accumulates by itself (normal keyboard).
    // Two keys in and still a single character means it's the compact one.
    if (g_kb.mode == 0) {
        if (key.size() > 1) g_kb.mode = 1;
        else if (g_kb.keys >= 2) g_kb.mode = 2;
    }

    if (g_kb.mode == 1) {
        g_kb.text = key;
    } else if (key == "\b" || key == "\x7f") {
        PopUtf8(g_kb.text);
    } else if (key == "\r" || key == "\n" || key == "\t") {
        // Enter/checkmark and tab come through as characters too; they are not part of the text.
    } else if (g_kb.text.size() + key.size() <= g_kb.maxBytes) {
        g_kb.text += key;
    }
    LogDebug("key %d: buffer=\"%s\" mode=%d text=\"%s\"", g_kb.keys, key.c_str(), g_kb.mode, g_kb.text.c_str());
    TextOverlayUpdate(static_cast<vr::IVROverlay*>(self), g_kb.text);
}

// ---- hooks -------------------------------------------------------------------------------

static vr::EVROverlayError HkShowKeyboardForOverlay(void* self, vr::VROverlayHandle_t /*overlay*/,
    vr::EGamepadTextInputMode inputMode, vr::EGamepadTextInputLineMode lineMode, const char* desc,
    uint32_t maxChars, const char* existing, bool minimal, uint64_t userValue)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    g_last.inputMode = inputMode;
    g_last.lineMode = lineMode;
    g_last.description = desc ? desc : "";
    g_last.existing = existing ? existing : "";
    g_last.maxChars = maxChars;

    // The overlay-bound keyboard never appears on the Steam Frame and never sends KeyboardDone,
    // so the game would wait forever. Open the plain keyboard with the same arguments instead;
    // its events still arrive through the game's PollNextOverlayEvent.
    vr::EVROverlayError r = OpenKeyboard(static_cast<vr::IVROverlay*>(self), inputMode, lineMode, desc,
                                         maxChars, existing, minimal, userValue);
    Log("game opened a keyboard (\"%s\", max %u): swapped to the standard keyboard, result %d",
        desc ? desc : "", maxChars, (int)r);
    return r;
}

static uint32_t HkGetKeyboardText(void* self, char* buf, uint32_t cch)
{
    uint32_t r = g_origGetText(self, buf, cch);
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    // Hand over the rebuilt text, unless the keyboard accumulates by itself (mode 1).
    // Nothing typed: leave SteamVR's answer alone, the game then keeps its default name.
    if (g_kb.active && g_kb.mode != 1 && !g_kb.text.empty() && buf && cch > 0) {
        size_t n = g_kb.text.size() < cch - 1 ? g_kb.text.size() : cch - 1;
        memcpy(buf, g_kb.text.data(), n);
        buf[n] = '\0';
        r = (uint32_t)n;
        LogDebug("GetKeyboardText: returned rebuilt text \"%s\"", buf);
    } else {
        LogDebug("GetKeyboardText: passed through (%u bytes)", r);
    }
    return r;
}

static void HkHideKeyboard(void* self)
{
    g_origHide(self);
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    CloseKeyboardState("hidden");
}

static bool HkPollNextOverlayEvent(void* self, vr::VROverlayHandle_t handle, vr::VREvent_t* ev, uint32_t cb)
{
    bool r = g_origPoll(self, handle, ev, cb);
    if (!r || !ev) return r;
    switch (ev->eventType) {
    case vr::VREvent_KeyboardCharInput: {
        std::lock_guard<std::recursive_mutex> guard(g_lock);
        if (g_kb.active) OnKeyboardChar(self);
        break;
    }
    case vr::VREvent_KeyboardDone:
        LogDebug("keyboard: Done");
        break;
    case vr::VREvent_KeyboardClosed: {
        std::lock_guard<std::recursive_mutex> guard(g_lock);
        CloseKeyboardState("closed");
        break;
    }
    }
    return r;
}

// ---- public ------------------------------------------------------------------------------

void KeyboardFixPatch(void* obj)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    if (!obj || obj == g_overlay) return;
    // The ShowKeyboardForOverlay swap goes last: it is the only hook that changes anything, and
    // it must not go live unless the hooks it relies on are in place.
    bool ok = PatchSlot(obj, kPollNextOverlayEvent, (void*)&HkPollNextOverlayEvent, (void**)&g_origPoll) &&
              PatchSlot(obj, kGetKeyboardText, (void*)&HkGetKeyboardText, (void**)&g_origGetText) &&
              PatchSlot(obj, kHideKeyboard, (void*)&HkHideKeyboard, (void**)&g_origHide) &&
              PatchSlot(obj, kShowKeyboardForOverlay, (void*)&HkShowKeyboardForOverlay, (void**)&g_origShowForOverlay);
    if (ok) g_overlay = static_cast<vr::IVROverlay*>(obj);
    Log("IVROverlay_018 patch: %s", ok ? "ok" : "FAILED, keyboard fix inactive");
}

void KeyboardFixReset()
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    g_kb.active = false;
    g_kb.text.clear();
    g_overlay = nullptr;
    TextOverlayReset();
}

void KeyboardSummon()
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    if (!g_overlay) { Log("hotkey: summon ignored, VR is not ready"); return; }
    vr::EVROverlayError r = OpenKeyboard(g_overlay, g_last.inputMode, g_last.lineMode, g_last.description.c_str(),
                                         g_last.maxChars, g_last.existing.c_str(), false, 0);
    Log("hotkey: summoned keyboard, result %d", (int)r);
}

void KeyboardHide()
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    if (!g_overlay) return;
    Log("hotkey: hide keyboard");
    HkHideKeyboard(g_overlay);
}
