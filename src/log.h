#pragma once

// One log per game session, overwritten on every launch (the usual SKSE convention):
//   <Documents>\My Games\Skyrim VR\SKSE\FrameKeyboardSummoner.log

void LogOpen();
void LogSetDebug(bool on);

// Always written: startup, config, hook status, keyboard open/close.
void Log(const char* fmt, ...);
// Only written with [General] DebugLog=1: per-keypress details for bug reports.
void LogDebug(const char* fmt, ...);
