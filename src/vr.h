#pragma once

#include "openvr.h"

// The real VR_GetGenericInterface (bypasses our hook). Returns nullptr if OpenVR isn't running.
void* VRInterface(const char* version);
