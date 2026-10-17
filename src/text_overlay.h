#pragma once

#include <string>
#include "openvr.h"

// The compact SteamVR keyboard on the Steam Frame has no text field, so you can't see what
// you typed. This draws one: a small SteamVR overlay that shows the rebuilt text.
// All functions do nothing when [TextOverlay] Enabled=0.

// The keyboard just opened: place the box in front of the current gaze and show it empty.
void TextOverlayOpen(vr::IVROverlay* overlay);
// Redraw with the current text.
void TextOverlayUpdate(vr::IVROverlay* overlay, const std::string& text);
void TextOverlayHide(vr::IVROverlay* overlay);
// OpenVR shut down: our overlay handle is gone, forget it.
void TextOverlayReset();
