#include "config/runtime_config.hpp"
#include "esp/esp.hpp"
#include "license/expiry.hpp"
#include "memory/process.hpp"
#include "render/overlay.hpp"
#include "stealth/stealth.hpp"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <random>
#include <thread>

#ifdef CS2_ESP_CONSOLE
#include <iostream>
#endif

namespace {

constexpr auto kActiveFramePeriod = std::chrono::microseconds(16667); // ~60 Hz
constexpr auto kEspFramePeriod = std::chrono::microseconds(16667);    // ~60 Hz; 120 Hz reads hitch the game
constexpr auto kCombatFramePeriod = std::chrono::microseconds(12500); // ~80 Hz when aiming/firing
constexpr auto kIdleSleep = std::chrono::milliseconds(400);
constexpr auto kForegroundHideDelay = std::chrono::milliseconds(2800);
constexpr auto kStartupEspWarmup = std::chrono::seconds(20);
constexpr auto kStartupWaitForGame = std::chrono::seconds(60);
constexpr auto kStartupPollInterval = std::chrono::milliseconds(50);
constexpr wchar_t kStartupMsgBoxTitle[] = L"CS2 ESP";

bool is_end_held() { return (GetAsyncKeyState(VK_END) & 0x8000) != 0; }

void close_startup_message_box() {
    if (HWND hwnd = FindWindowW(nullptr, kStartupMsgBoxTitle))
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

bool key_pressed_once(int vk) {
    static SHORT last[256]{};
    const SHORT state = GetAsyncKeyState(vk);
    const bool down = (state & 0x8000) != 0;
    const bool was_down = (last[vk] & 0x8000) != 0;
    last[vk] = state;
    return down && !was_down;
}

void attach_loop(memory::Process& process) {
    while (!process.attach(L"cs2.exe")) {
#ifdef CS2_ESP_CONSOLE
        std::cout << "[*] Waiting for cs2.exe ...\n";
#endif
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
#ifdef CS2_ESP_CONSOLE
    std::cout << "[+] Attached. client.dll = 0x" << std::hex << process.client_base() << std::dec
              << '\n';
#endif
}

bool wait_for_cs2_at_startup(memory::Process& process) {
    if (process.attach(L"cs2.exe")) {
#ifdef CS2_ESP_CONSOLE
        std::cout << "[+] Attached. client.dll = 0x" << std::hex << process.client_base()
                  << std::dec << '\n';
#endif
        return true;
    }

    std::atomic<bool> msg_box_done{false};
    std::thread msg_box_thread([&] {
        MessageBoxW(nullptr,
                    L"\u672a\u68c0\u6d4b\u5230 Counter-Strike 2\uff08cs2.exe\uff09\u3002\n\u8bf7\u5148\u542f"
                    L"\u52a8\u6e38\u620f\uff1b\u7a0b\u5e8f\u5c06\u5728 60 \u79d2\u5185\u7b49\u5f85\uff0c\u671f"
                    L"\u95f4\u542f\u52a8\u6e38\u620f\u53ef\u81ea\u52a8\u7ee7\u7eed\u3002\n\u6309 End \u53ef\u7acb"
                    L"\u5373\u9000\u51fa\uff08\u65e0\u9700\u5173\u95ed\u672c\u63d0\u793a\uff09\u3002",
                    kStartupMsgBoxTitle, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        msg_box_done = true;
    });

#ifdef CS2_ESP_CONSOLE
    std::cout << "[*] CS2 not running — waiting up to 60s (End to quit) ...\n";
#endif

    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + kStartupWaitForGame;
    while (clock::now() < deadline) {
        if (is_end_held()) {
            close_startup_message_box();
            if (msg_box_thread.joinable())
                msg_box_thread.join();
            return false;
        }

        if (process.attach(L"cs2.exe")) {
            close_startup_message_box();
            if (msg_box_thread.joinable())
                msg_box_thread.join();
#ifdef CS2_ESP_CONSOLE
            std::cout << "[+] Attached. client.dll = 0x" << std::hex << process.client_base()
                      << std::dec << '\n';
#endif
            return true;
        }

        std::this_thread::sleep_for(kStartupPollInterval);
    }

    close_startup_message_box();
    if (msg_box_thread.joinable())
        msg_box_thread.join();

    MessageBoxW(nullptr,
                L"\u7b49\u5f85\u8d85\u65f6\uff1a\u4ecd\u672a\u68c0\u6d4b\u5230 Counter-Strike 2\uff0c\u7a0b"
                L"\u5e8f\u5c06\u9000\u51fa\u3002",
                L"CS2 ESP", MB_OK | MB_ICONWARNING | MB_TOPMOST);
#ifdef CS2_ESP_CONSOLE
    std::cout << "[!] Timed out waiting for cs2.exe\n";
#endif
    return false;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!license::ensure_license_valid())
        return 1;

#ifdef CS2_ESP_CONSOLE
    AllocConsole();
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
#ifdef CS2_ESP_ESP_ONLY
    std::cout << "Insert box ESP | PageUp skeleton | End quit\n";
#else
    std::cout << "Insert box ESP | PageUp skeleton | Home toggle memory aim | Hold XBUTTON1 aim | End quit\n";
#endif
#endif

    memory::Process process;
    if (!wait_for_cs2_at_startup(process))
        return 0;

    render::Overlay overlay;
    if (!overlay.create()) {
#ifdef CS2_ESP_CONSOLE
        std::cerr << "[!] Overlay create failed\n";
#endif
        return 1;
    }

    esp::EspLoop esp_loop(process);
    config::write_default_runtime_config_if_missing();

    esp_loop.show_hint(
        L"\u5df2\u8fde\u63a5 CS2\u3002Alt+Tab \u79bb\u5f00\u65f6\u6682\u505c\u7ed8\u5236\uff0c"
        L"\u8fdb\u7a0b\u4ecd\u5728\u540e\u53f0\u8fd0\u884c\uff1b\u5207\u56de\u6e38\u620f\u6062\u590d\u3002"
        L"End \u9000\u51fa\u3002",
        std::chrono::seconds(12));

    using clock = std::chrono::steady_clock;
    const auto session_started = clock::now();
    bool prev_game_active = false;
    bool saw_game_active = false;
    auto last_attach_check = clock::now();
    auto last_license_check = clock::now();
    auto last_game_foreground = clock::now();
    auto next_frame = clock::now();

    std::mt19937 rng(static_cast<unsigned>(GetTickCount64() ^ GetCurrentProcessId()));
    std::uniform_int_distribution<int> handle_refresh_sec(38, 52);
    auto next_handle_refresh =
        clock::now() + std::chrono::seconds(handle_refresh_sec(rng));

    const auto ignore_end_until = clock::now() + std::chrono::seconds(3);

    while (true) {
        if (key_pressed_once(VK_END) && clock::now() >= ignore_end_until)
            break;

        if (key_pressed_once(VK_INSERT))
            esp_loop.toggle();

        if (key_pressed_once(VK_PRIOR))
            esp_loop.toggle_skeleton();

        // Page Down 烟闪屏蔽：功能已关闭，实现保留在 entity.cpp（kUtilitySuppressFeatureEnabled）
        // if (key_pressed_once(VK_NEXT))
        //     esp_loop.toggle_utility_suppress();

#ifndef CS2_ESP_ESP_ONLY
        if (key_pressed_once(esp_loop.aim_toggle_vk()))
            esp_loop.toggle_aim();
#endif

        if (key_pressed_once(VK_F7))
            esp_loop.toggle_color_picker(overlay);
        if (esp_loop.color_picker_visible() && key_pressed_once(VK_ESCAPE))
            esp_loop.set_color_picker_visible(overlay, false);
        if (esp_loop.color_picker_visible() && key_pressed_once(VK_TAB))
            esp_loop.toggle_color_picker_tab();

        esp_loop.poll_runtime_config();

        const auto now = clock::now();

        if (now - last_attach_check > std::chrono::seconds(5)) {
            if (!process.is_attached())
                attach_loop(process);
            last_attach_check = now;
        }

#ifdef CS2_ESP_RETAIL
        if (now - last_license_check > std::chrono::minutes(1)) {
            if (!license::ensure_license_valid())
                break;
            last_license_check = now;
        }
#endif

        const DWORD cs2_pid = process.process_id();
        const bool cs2_active = cs2_pid != 0 && stealth::is_game_window_active(cs2_pid);
        if (cs2_active)
            last_game_foreground = now;

        const bool warmup =
            cs2_pid != 0 && now - session_started < kStartupEspWarmup &&
            stealth::is_game_window_available(cs2_pid);
        const bool game_active =
            cs2_pid != 0 &&
            (cs2_active || warmup || now - last_game_foreground < kForegroundHideDelay);

        if (esp_loop.needs_write_access_from_input())
            process.ensure_write_access();
        else
            process.downgrade_to_read_only();

        const bool refresh_due_io = process.last_io_failed();
        const bool refresh_due_timer =
            game_active && esp_loop.wants_scheduled_handle_refresh() && now >= next_handle_refresh;
        if ((refresh_due_io || refresh_due_timer) && process.is_attached()) {
            process.refresh_handle();
            process.clear_io_failed();
            next_handle_refresh = now + std::chrono::seconds(handle_refresh_sec(rng));
        }

        if (game_active && saw_game_active && !prev_game_active) {
            esp_loop.show_hint(
                L"\u5df2\u5207\u56de CS2\uff0c\u663e\u793a\u5df2\u6062\u590d\uff08\u672a\u9000\u51fa\uff0cEnd \u53ef\u5173\u95ed\u7a0b\u5e8f\uff09",
                std::chrono::seconds(5));
        }
        if (game_active)
            saw_game_active = true;
        prev_game_active = game_active;

        if (game_active) {
            esp_loop.tick(overlay, true);

            const bool esp_drawing = esp_loop.enabled() || esp_loop.skeleton_enabled();
#ifndef CS2_ESP_ESP_ONLY
            const bool combat =
                esp_loop.aim_toggled() ||
                (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0 ||
                (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const auto period =
                combat ? kCombatFramePeriod : (esp_drawing ? kEspFramePeriod : kActiveFramePeriod);
#else
            const auto period = esp_drawing ? kEspFramePeriod : kActiveFramePeriod;
#endif
            next_frame += stealth::jittered_frame_period(period, rng);
            if (next_frame > now)
                std::this_thread::sleep_until(next_frame);
            else
                next_frame = now;
        } else {
            esp_loop.tick(overlay, false);
            std::this_thread::sleep_for(kIdleSleep);
            next_frame = clock::now();
        }

        overlay.pump_messages();
    }

    overlay.destroy();
    return 0;
}
