#pragma once

// Settings from FrameKeyboardSummoner.ini, next to the DLL in Data\SKSE\Plugins.
// The file is only read, never written, so MO2 keeps it inside the mod folder.
// Missing file or keys fall back to the defaults below. Read once at startup.
struct Config {
    bool debugLog = false;    // [General] DebugLog
    bool keyboardFix = true;  // [Keyboard] Enabled: master switch for the whole plugin

    struct {
        bool  enabled = true;     // show the typed-text box above the keyboard
        bool  followHead = false; // false: stays where you looked when the keyboard opened
        float width = 0.6f;       // meters
        float distance = 1.0f;    // meters in front of the headset
        float offsetX = 0.0f;     // meters right (+) / left (-) of gaze
        float offsetY = -0.03f;   // meters above (+) / below (-) gaze
    } overlay;

    struct {
        bool enabled = false; // Ctrl+Alt hotkeys, off by default
        int  summonKey = 'K'; // virtual-key code (A-Z and 0-9 match their ASCII code)
        int  hideKey = 'H';
    } hotkeys;
};

extern Config g_cfg;

void LoadConfig();
