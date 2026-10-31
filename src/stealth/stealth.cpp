#include "stealth/stealth.hpp"

#include <array>
#include <cstdio>

namespace stealth {

std::wstring random_overlay_class_name() {
    const unsigned seed =
        static_cast<unsigned>(GetTickCount64() ^ (GetCurrentProcessId() << 16));
    std::mt19937 rng(seed);

    wchar_t buf[48]{};
    std::uniform_int_distribution<int> hex(0, 15);
    const wchar_t* digits = L"0123456789ABCDEF";
    for (int i = 0; i < 32; ++i)
        buf[i] = digits[hex(rng)];
    buf[32] = L'\0';

    wchar_t out[64]{};
    swprintf_s(out, L"UI_%s", buf);
    return out;
}

namespace {

bool hwnd_belongs_to_process(const HWND hwnd, const DWORD process_id) {
    if (!hwnd || process_id == 0)
        return false;
    DWORD window_pid = 0;
    GetWindowThreadProcessId(hwnd, &window_pid);
    return window_pid == process_id;
}

struct EnumWindowData {
    DWORD pid{};
    HWND best{};
    int best_area{};
};

BOOL CALLBACK enum_largest_client_window(HWND hwnd, LPARAM lparam) {
    auto* data = reinterpret_cast<EnumWindowData*>(lparam);
    if (!IsWindowVisible(hwnd))
        return TRUE;

    DWORD wnd_pid = 0;
    GetWindowThreadProcessId(hwnd, &wnd_pid);
    if (data->pid != 0 && wnd_pid != data->pid)
        return TRUE;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc))
        return TRUE;
    const int area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > data->best_area) {
        data->best_area = area;
        data->best = hwnd;
    }
    return TRUE;
}

HWND find_cs2_root_window(const DWORD process_id) {
    if (process_id == 0)
        return nullptr;

    if (HWND by_title = FindWindowW(nullptr, L"Counter-Strike 2");
        by_title && hwnd_belongs_to_process(by_title, process_id))
        return by_title;

    EnumWindowData data{};
    data.pid = process_id;
    EnumWindows(enum_largest_client_window, reinterpret_cast<LPARAM>(&data));
    if (data.best)
        return data.best;

    if (HWND sdl = FindWindowW(L"SDL_app", nullptr);
        sdl && hwnd_belongs_to_process(sdl, process_id))
        return sdl;

    return nullptr;
}

} // namespace

bool is_process_foreground(const DWORD process_id) {
    if (process_id == 0)
        return false;

    const HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    DWORD fg_pid = 0;
    GetWindowThreadProcessId(fg, &fg_pid);
    return fg_pid == process_id;
}

bool is_game_window_available(const DWORD process_id) {
    if (process_id == 0)
        return false;
    const HWND game = find_cs2_root_window(process_id);
    return game && IsWindow(game) && !IsIconic(game);
}

bool is_game_window_active(const DWORD process_id) {
    if (process_id == 0)
        return false;

    const HWND game = find_cs2_root_window(process_id);
    if (!game || IsIconic(game))
        return false;

    const HWND fg = GetForegroundWindow();
    if (!fg)
        return true;

    if (hwnd_belongs_to_process(fg, process_id))
        return true;

    const HWND fg_root = GetAncestor(fg, GA_ROOT);
    if (fg_root == game)
        return true;

    if (fg_root && hwnd_belongs_to_process(fg_root, process_id))
        return true;

    return false;
}

std::chrono::microseconds jittered_frame_period(const std::chrono::microseconds base,
                                                std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(-1500, 1500);
    const auto jitter = std::chrono::microseconds(dist(rng));
    const auto result = base + jitter;
    if (result.count() < 8000)
        return std::chrono::microseconds(8000);
    return result;
}

} // namespace stealth
