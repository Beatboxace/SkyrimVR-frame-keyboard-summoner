#include "hotkeys.h"
#include "config.h"
#include "keyboard_fix.h"
#include "log.h"

#include <windows.h>

static bool Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// Only these two chords are ever checked; no other key state is read or stored.
static bool Chord(int vk) { return Down(VK_CONTROL) && Down(VK_MENU) && Down(vk); }

// Polling interval: short enough to catch a normal key tap, and a negligible cost.
static const DWORD kPollMs = 50;

static DWORD WINAPI HotkeyThread(LPVOID)
{
    const int summonKey = g_cfg.hotkeys.summonKey, hideKey = g_cfg.hotkeys.hideKey;
    bool wasSummon = false, wasHide = false;
    for (;;) {
        Sleep(kPollMs);
        bool summon = Chord(summonKey), hide = Chord(hideKey);
        if (summon && !wasSummon) KeyboardSummon(); // act once per press, not while held
        if (hide && !wasHide) KeyboardHide();
        wasSummon = summon;
        wasHide = hide;
    }
}

void HotkeysStart()
{
    if (!g_cfg.hotkeys.enabled) { Log("hotkeys: off"); return; }
    HANDLE t = CreateThread(nullptr, 0, HotkeyThread, nullptr, 0, nullptr);
    if (!t) { Log("hotkeys: could not start thread"); return; }
    CloseHandle(t);
    Log("hotkeys: on, Ctrl+Alt+%c summons the keyboard, Ctrl+Alt+%c hides it",
        g_cfg.hotkeys.summonKey, g_cfg.hotkeys.hideKey);
}
