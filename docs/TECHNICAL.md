# Technical notes

How Frame Keyboard Summoner works, what was found while debugging, and what to watch out for when changing it.

## Environment it was found on
- Valve Steam Frame streaming from a Windows PC through SteamVR.
- Skyrim VR 1.4.15, SKSEVR, MO2 with a large modlist (FUS).
- The game's `openvr_api.dll` exposes `IVRSystem_019`, `IVRCompositor_022`, `IVROverlay_018`. The matching header is
  ValveSoftware/openvr commit `6aacebd1246592d9911439d5abd0c657b8948ab0` (`third_party/openvr/openvr.h`).
- This was tested using the FUS modlist, so some values might differ depending on mods used.

## Root cause
The game opens its text prompt with `IVROverlay::ShowKeyboardForOverlay` (call site `SkyrimVR.exe+0x8E1176`, args seen
for the name prompt: `desc="RaceSex Menu"`, `unCharMax=26`, `existing="Shezarrine"`, `minimal=false`, `user=0`).
The call returns `VROverlayError_None`, but on the Frame SteamVR never renders an overlay-bound keyboard and never sends
`VREvent_KeyboardDone`, so the game waits forever. Plain `IVROverlay::ShowKeyboard` with the same arguments does render.

When the keyboard is done, the game reads the result with `GetKeyboardText` (call site `SkyrimVR.exe+0x8DDFFF`, `cch=26`)
on `VREvent_KeyboardDone`.

## Event flow (after the swap)
Keyboard events for the plain keyboard still arrive through the game's `PollNextOverlayEvent` on its own overlay handle,
on the game's main thread. The game never calls `IVRSystem::PollNextEvent`.

```
ShowKeyboardForOverlay  (game)  -> we call ShowKeyboard instead
VREvent_KeyboardCharInput 1201  -> one per key, data is empty
VREvent_KeyboardDone      1202  -> game calls GetKeyboardText
VREvent_KeyboardClosed    1200
```

## The compact keyboard
SteamVR gives the Frame a compact keyboard with no text field. Each `KeyboardCharInput` carries no text, and
`GetKeyboardText` holds **only the last key pressed** (on Done it holds `"\n"` from the checkmark key).

The accumulator in `keyboard_fix.cpp` reads `GetKeyboardText` after every `KeyboardCharInput`:
- buffer longer than one char → the keyboard accumulates by itself (mode 1, used as is);
- still one char after two keys → compact keyboard (mode 2): append each key, `\b`/`0x7F` removes the last UTF-8
  character, `\r \n \t` are ignored, the text is capped at `unCharMax - 1` bytes.

The hooked `GetKeyboardText` returns the rebuilt text when it isn't empty. Empty text is passed through, and the game then keeps its default name.

Tried and failed to get the full keyboard: other input/line modes, `bUseMinimalMode`, empty/non-empty existing text,
controller binding layouts. The typed-text box is the substitute.

## Hooks
MinHook on `openvr_api.dll` exports: `VR_GetGenericInterface`, `VR_InitInternal`, `VR_InitInternal2`, `VR_ShutdownInternal`.
After a successful init (and whenever someone requests `IVROverlay_018`) the interface vtable is patched directly:

| IVROverlay_018 slot | Index | Hook |
|---|---|---|
| PollNextOverlayEvent | 45 | watch keyboard events |
| ShowKeyboard | 72 | not hooked, called by us |
| ShowKeyboardForOverlay | 73 | swapped to ShowKeyboard |
| GetKeyboardText | 74 | return rebuilt text |
| HideKeyboard | 75 | close our state and the box |

Other indices (for reference): IsDashboardVisible 66, SetKeyboardPositionForOverlay 77, IVRSystem_019 PollNextEvent 30.
The indices **must** come from the header at the commit above, not from a newer SDK.

Patching rules: a slot that already holds our hook is skipped (re-init can return the same vtable); the
`ShowKeyboardForOverlay` slot is patched last, so the swap never goes live without the other hooks in place.
The accumulator and the box only run while a keyboard *we* opened is active.

## Typed-text box
1024×192 RGBA drawn with GDI, uploaded with `SetOverlayRaw`, overlay key `framekeyboardsummoner.typedtext`, sort order 1000.
It is placed once per keyboard open: either world-fixed at the head pose in `TrackingUniverseStanding` (default), or
head-relative. The 6 px gray border is inset 8 px and the outer 8 px fade to transparent, because a 1 px border aliased badly in the headset.

## Gotchas
- `PrivateProfileRedirector` (common in modlists) hooks `GetPrivateProfile*`/`WritePrivateProfile*`, which is why the INI is
  parsed with plain file I/O.
- Error 109 and an `IVRSystem_022` request at exit come from `VR_ShutdownInternal` followed by CrashLogger. Harmless.
- Windows `min`/`max` macros: the build defines `NOMINMAX`.
- Summoning a keyboard when the game isn't waiting for one can make the game take it as the answer to its next prompt.

## Not done / ideas
- Positioning the keyboard itself (`SetKeyboardPositionForOverlay` / `SetKeyboardTransformAbsolute`). It opens where you look, which is fine.
- Phase 2: Steam Frame adapted controller bindings. Will probably come in a separate mod.
