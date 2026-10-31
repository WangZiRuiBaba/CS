#pragma once

#include <Windows.h>

#include <chrono>
#include <random>
#include <string>

namespace stealth {

std::wstring random_overlay_class_name();

bool is_process_foreground(DWORD process_id);

// Borderless windowed: treat CS2 root window as active even if a child hwnd has focus.
bool is_game_window_active(DWORD process_id);

// Game window exists and is not minimized (ignores foreground — for startup / overlay focus).
bool is_game_window_available(DWORD process_id);

std::chrono::microseconds jittered_frame_period(std::chrono::microseconds base,
                                                std::mt19937& rng);

} // namespace stealth
