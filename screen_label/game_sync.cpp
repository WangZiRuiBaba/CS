#include "game_sync.hpp"
#include "overlay.h"

namespace screen_label {

namespace {

struct EnumWindowData {
    DWORD pid{};
    HWND best{};
    int best_area{};
};

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam) {
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

bool hwnd_belongs_to_process(HWND hwnd, DWORD process_id) {
    if (!hwnd || process_id == 0)
        return false;
    DWORD window_pid = 0;
    GetWindowThreadProcessId(hwnd, &window_pid);
    return window_pid == process_id;
}

HWND find_cs2_window(DWORD game_process_id) {
    if (HWND by_title = FindWindowW(nullptr, L"Counter-Strike 2");
        by_title && hwnd_belongs_to_process(by_title, game_process_id))
        return by_title;

    EnumWindowData data{};
    data.pid = game_process_id;
    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&data));
    if (data.best)
        return data.best;

    if (HWND sdl = FindWindowW(L"SDL_app", nullptr);
        sdl && hwnd_belongs_to_process(sdl, game_process_id))
        return sdl;

    return nullptr;
}

} // namespace

bool sync_overlay_to_cs2(GdiOverlay& overlay, DWORD game_process_id) {
    if (game_process_id == 0)
        return false;

    HWND game = find_cs2_window(game_process_id);
    if (!game || !IsWindow(game))
        return false;

    RECT client{};
    if (!GetClientRect(game, &client))
        return false;

    POINT top_left{ client.left, client.top };
    ClientToScreen(game, &top_left);

    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    if (w <= 0 || h <= 0)
        return false;

    return overlay.Resize(top_left.x, top_left.y, w, h);
}

} // namespace screen_label
