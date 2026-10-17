#pragma once

// The actual fix. Skyrim VR opens its text prompt (character name, etc.) with
// IVROverlay::ShowKeyboardForOverlay. On the Steam Frame that keyboard never shows up and the game
// waits forever. We patch the IVROverlay_018 vtable so the call opens the plain ShowKeyboard instead,
// then rebuild the typed text, because the compact keyboard only reports the last key pressed.

// Patch an IVROverlay_018 instance. Safe to call repeatedly with the same object.
void KeyboardFixPatch(void* overlay);
// OpenVR is shutting down: drop the interface pointer and close our state.
void KeyboardFixReset();

// For the optional hotkeys (called from the hotkey thread).
void KeyboardSummon();
void KeyboardHide();
