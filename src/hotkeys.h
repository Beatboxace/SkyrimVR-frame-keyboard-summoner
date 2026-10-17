#pragma once

// Optional emergency hotkeys ([Hotkeys] Enabled=1): Ctrl+Alt+SummonKey opens the keyboard,
// Ctrl+Alt+HideKey closes it. Meant to recover a text prompt that is stuck without a keyboard.
// Starts a small polling thread; does nothing unless enabled in the INI.
void HotkeysStart();
