# Frame Keyboard Summoner

**Fixes Skyrim VR getting stuck at the character name (and other text prompts) on the Valve Steam Frame.**

An SKSEVR plugin. No game files are edited, no ESP, no load order slot.

---

## The problem

When you finish character creation and press **OK** to name your character, the controller turns into a
laser pointer as if a keyboard opened, but **no keyboard ever appears** on the Steam Frame. The game waits
for a name that can never be typed, and you're stuck.

Why: Skyrim VR asks SteamVR for a keyboard *attached to its menu overlay*
(`IVROverlay::ShowKeyboardForOverlay`). On the Steam Frame, SteamVR accepts the request but never shows that
keyboard. The plain SteamVR keyboard (`ShowKeyboard`) *does* work there, and the game just never asks for it.

## What this mod does

1. **Swaps the keyboard.** When the game asks for the overlay keyboard, the mod opens the standard SteamVR
   keyboard with the same settings (description, max length, default name).
2. **Rebuilds what you type.** The compact keyboard you get on the Frame only reports the last key pressed,
   so the game would receive a one-letter name. The mod records every keypress (backspace included)
   and gives the game the full text when you confirm.
3. **Shows what you type.** That compact keyboard has no text field, so the mod displays a small box
   above it with your text and a cursor.

Press the **checkmark (✔)** key to confirm. If you confirm without typing anything, the game keeps its default name.

The mod only hooks SteamVR's OpenVR keyboard functions. It doesn't touch game code, so it doesn't depend on
game-version offsets or the Address Library.

## Requirements

| | |
|---|---|
| Game | Skyrim VR (1.4.15) |
| [SKSEVR](https://skse.silverlock.org/) | Required (2.0.12 for 1.4.15) |
| Runtime | **SteamVR** (Steam Frame streaming through SteamVR). Other setups might work (untested). |
| Not compatible | **OpenComposite** (replaces SteamVR's `openvr_api.dll` and has no SteamVR keyboard or overlays) |

Other headsets: you don't need this mod if your keyboard already works. It is harmless if installed anyway
(see [Configuration](#configuration) to turn it off without uninstalling).

## Installation

### Mod Organizer 2 (recommended)
1. Download the archive (`FrameKeyboardSummoner-x.y.z.zip`).
2. In MO2: **Install a new mod from an archive** (the box icon) → pick the zip → OK.
3. Enable the mod in the left pane. Load order / priority doesn't matter; it has no plugin (ESP) to place.
4. Launch Skyrim VR through **SKSE** from MO2 as usual.

### Vortex
Drag the zip into the Vortex *Mods* page, install, enable, deploy. Launch through SKSE.

### Manual
Extract the zip into the game's `Data` folder so you end up with:
```
<Skyrim VR>\Data\SKSE\Plugins\FrameKeyboardSummoner.dll
<Skyrim VR>\Data\SKSE\Plugins\FrameKeyboardSummoner.ini
```

### Check that it works
Start a new game, reach the name prompt: the SteamVR keyboard should open with the text box above it.
Afterwards, `Documents\My Games\Skyrim VR\SKSE\FrameKeyboardSummoner.log` should contain
`IVROverlay_018 patch: ok` and `game opened a keyboard ... swapped to the standard keyboard`.

## Configuration

Settings are in `SKSE\Plugins\FrameKeyboardSummoner.ini`, shipped with the mod.
With MO2: right-click the mod → **Open in Explorer** → `SKSE\Plugins`, and edit it there.
The file is read once at startup, so restart the game after changing it. A missing key uses the default.

| Section / key | Default | What it does / why change it |
|---|---|---|
| `[General] DebugLog` | `0` | `1` logs every keypress and what the game receives. Turn on before reporting a bug. |
| `[Keyboard] Enabled` | `1` | Master switch. `0` makes the plugin do nothing, so you can rule it out when troubleshooting. |
| `[TextOverlay] Enabled` | `1` | `0` hides the typed-text box. Typing still works, you just can't see the text. |
| `[TextOverlay] FollowHead` | `0` | `0`: the box stays where you were looking when the keyboard opened, like the keyboard. `1`: it moves with your head. |
| `[TextOverlay] WidthMeters` | `0.6` | Box size. Bigger is easier to read, smaller hides less of the keyboard. |
| `[TextOverlay] DistanceMeters` | `1.0` | How far in front of you the box is. |
| `[TextOverlay] OffsetX` | `0.0` | Moves the box right (+) or left (-), in meters. |
| `[TextOverlay] OffsetY` | `-0.03` | Moves the box up (+) or down (-), in meters. Adjust if it overlaps the keyboard. |
| `[Hotkeys] Enabled` | `0` | Turns on the emergency hotkeys below. |
| `[Hotkeys] SummonKey` | `K` | Ctrl+Alt+this key opens the keyboard. A single letter or digit. |
| `[Hotkeys] HideKey` | `H` | Ctrl+Alt+this key closes the keyboard. |

## Emergency hotkeys (optional, off by default)

If a text prompt is open but no keyboard is visible (for example, it was closed by accident), you can bring it back
from the **PC keyboard**:

1. Set `[Hotkeys] Enabled=1` in the INI and restart the game.
2. **Ctrl+Alt+K** opens the keyboard (with the text box), using the same settings the game last asked for.
3. **Ctrl+Alt+H** closes it.

> ⚠️ Use these **only while the game is waiting at a text prompt.** If you summon a keyboard at any other moment,
> the game can treat what you type as an answer to its next prompt and skip steps (for example during character creation).

When enabled, a small background check looks at the keys every 50 ms, even when the game window isn't focused.
Only those two Ctrl+Alt key combinations are checked; nothing else is read or stored.
When disabled (the default), that check never starts: no thread, no key reading.

## Files & logs

| File | Location | Notes |
|---|---|---|
| `FrameKeyboardSummoner.dll` | `Data\SKSE\Plugins\` (with MO2: inside the mod folder) | The plugin. |
| `FrameKeyboardSummoner.ini` | next to the DLL | Settings. The mod only reads it, never writes it, so nothing ends up in MO2's `overwrite`. |
| `FrameKeyboardSummoner.log` | `Documents\My Games\Skyrim VR\SKSE\` | Overwritten each launch. Short by default; `DebugLog=1` for details. Your typed text only appears in debug mode. |
| `FrameKeyboardSummoner_ThirdPartyNotices.txt` | next to the DLL | Licenses of the bundled libraries. |

When reporting a problem, set `DebugLog=1`, reproduce it once, and attach `FrameKeyboardSummoner.log`.

## Troubleshooting

- **No log file at all** → SKSE isn't loading the plugin. Make sure you launch through SKSE (`sksevr_loader.exe`),
  and check `Documents\My Games\Skyrim VR\SKSE\sksevr.log` for `FrameKeyboardSummoner`.
- **Log says `openvr_api.dll is not loaded` or the patch failed** → something replaced SteamVR's `openvr_api.dll`.
  Check the game folder and, with MO2 + Root Builder, `overwrite\Root` and your mods' `Root` folders for a stray
  `openvr_api.dll` (often left over from OpenComposite). Use the original file from Steam (*Verify integrity of game files*).
- **Keyboard opens but the name is wrong** → enable `DebugLog=1`, try again, and send the log.
- **The text box overlaps the keyboard** → change `OffsetY` / `DistanceMeters`, or set `[TextOverlay] Enabled=0`.
- **Other problems?** [Submit an issue](https://github.com/Beatboxace/SkyrimVR-frame-keyboard-summoner/issues) with
  reproduction steps (and your log with `DebugLog=1`), and I might be able to help.

## Known limits

- On the Frame, SteamVR shows the compact keyboard, which has no built-in text field. There's no known way
  to get the full keyboard; the text box replaces the missing field.
- Only the SteamVR runtime is supported (not OpenComposite / OpenXR layers).

## Uninstall

Disable or remove the mod in MO2/Vortex, or delete the two `FrameKeyboardSummoner.*` files from
`Data\SKSE\Plugins`. Saves aren't affected: the mod stores nothing in them.

## Building from source

Requirements: Windows, Visual Studio 2022 (or Build Tools) with the C++ workload, CMake 3.21+, Git.

```powershell
git clone https://github.com/Beatboxace/SkyrimVR-frame-keyboard-summoner.git
cd SkyrimVR-frame-keyboard-summoner
cmake -B build -A x64
cmake --build build --config Release                         # build\Release\FrameKeyboardSummoner.dll
cmake --build build --config Release --target package_zip    # dist\FrameKeyboardSummoner-<version>.zip
```

MinHook is downloaded automatically by CMake (pinned to v1.3.4). The OpenVR header is vendored in
`third_party/openvr` at the version matching Skyrim VR's runtime. The DLL links the C runtime statically.
The version number lives in `CMakeLists.txt` and is written into a generated `version.h` at configure time.
In VS Code, open the repo folder with the CMake Tools extension and run **CMake: Configure** once so IntelliSense finds it.

Source layout:

| File | Role |
|---|---|
| `src/main.cpp` | SKSE entry points, hooks on `openvr_api.dll` exports to find and patch the overlay interface |
| `src/keyboard_fix.cpp` | The fix: keyboard swap, text rebuilding, keyboard events |
| `src/text_overlay.cpp` | The typed-text box (drawn with GDI, shown as a SteamVR overlay) |
| `src/hotkeys.cpp` | Optional Ctrl+Alt hotkeys |
| `src/config.cpp`, `src/log.cpp` | INI reading and logging |

How it works in detail: [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Other notes
> This mod was built quickly with AI assistance, then code reviewed and tested in the headset by a human. The full source is here for anyone to check.

## Credits & license

- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu (BSD 2-Clause)
- [OpenVR SDK](https://github.com/ValveSoftware/openvr) headers by Valve (BSD 3-Clause)
- The SKSEVR team

Frame Keyboard Summoner is released under the [MIT License](LICENSE).
