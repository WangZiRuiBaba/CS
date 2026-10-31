#include "color_picker.h"
#include "game_sync.hpp"
#include "overlay.h"

#include "memory/process.hpp"
#include "sdk/entity.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr wchar_t kAppTitle[] = L"CS2 屏幕空间标注";

bool key_pressed_once(int vk) {
    static SHORT last[256]{};
    const SHORT state = GetAsyncKeyState(vk);
    const bool down = (state & 0x8000) != 0;
    const bool was = (last[vk] & 0x8000) != 0;
    last[vk] = state;
    return down && !was;
}

void pump_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void draw_target_box(GdiOverlay& overlay, const sdk::EspTarget& t, COLORREF color) {
    Box box{
        static_cast<int>(t.x),
        static_cast<int>(t.y),
        static_cast<int>(t.x + t.w),
        static_cast<int>(t.y + t.h),
    };
    overlay.DrawBox(box, color);
}

void draw_target_skeleton(GdiOverlay& overlay, const sdk::EspTarget& t, COLORREF color) {
    for (const sdk::SkeletonLine& line : t.skeleton) {
        overlay.DrawLine(static_cast<int>(line.x1), static_cast<int>(line.y1),
                         static_cast<int>(line.x2), static_cast<int>(line.y2), color, 1);
    }
    int hx = static_cast<int>(t.x + t.w * 0.5f);
    int hy = static_cast<int>(t.y + t.h * 0.08f);
    if (t.skeleton_head) {
        hx = static_cast<int>(t.skeleton_head->x);
        hy = static_cast<int>(t.skeleton_head->y);
    }
    const int radius = std::max(3, std::min(static_cast<int>(t.h * 0.045f), 7));
    overlay.DrawCircle(hx, hy, radius, color, 1);
}

void format_rgb(wchar_t* buf, size_t cap, COLORREF c) {
    swprintf_s(buf, cap, L"RGB(%u,%u,%u)", GetRValue(c), GetGValue(c), GetBValue(c));
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    GdiOverlay overlay;
    if (!overlay.Create()) {
        MessageBoxW(nullptr, L"Overlay 创建失败。", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    memory::Process process;
    std::unique_ptr<sdk::EntitySystem> entities;
    screen_label::ColorPickerUi color_picker;

    bool enabled = true;
    bool show_box = true;
    bool show_skeleton = false;
    COLORREF box_color = screen_label::ColorPickerUi::swatch_color(0);
    COLORREF skel_color = screen_label::ColorPickerUi::swatch_color(4);
    ULONGLONG last_attach_hint = 0;

    MessageBoxW(nullptr,
                L"Insert 总开关 · Home 方框 · PageUp 骨骼\n"
                L"F7 颜色面板 · Tab 切换预设/自定义\n"
                L"自定义：点方框/骨骼目标，拖色盘与色相条\n"
                L"End 退出",
                kAppTitle, MB_OK | MB_ICONINFORMATION);

    for (;;) {
        pump_messages();

        if (key_pressed_once(VK_END))
            break;
        if (key_pressed_once(VK_INSERT))
            enabled = !enabled;
        if (key_pressed_once(VK_HOME))
            show_box = !show_box;
        if (key_pressed_once(VK_PRIOR))
            show_skeleton = !show_skeleton;

        if (key_pressed_once(VK_F7)) {
            const bool next = !color_picker.visible();
            color_picker.set_visible(next);
            overlay.SetClickThrough(!next);
            if (next)
                color_picker.on_open(box_color, skel_color);
        }
        if (color_picker.visible() && key_pressed_once(VK_ESCAPE)) {
            color_picker.set_visible(false);
            overlay.SetClickThrough(true);
        }
        if (color_picker.visible() && key_pressed_once(VK_TAB))
            color_picker.toggle_tab();

        if (!process.is_attached()) {
            if (!process.attach(L"cs2.exe")) {
                if (GetTickCount64() - last_attach_hint > 8000) {
                    last_attach_hint = GetTickCount64();
                    MessageBoxW(nullptr,
                                L"未检测到 cs2.exe。\n请先启动 Counter-Strike 2，点确定后继续等待。",
                                kAppTitle, MB_OK | MB_ICONINFORMATION);
                }
                overlay.BeginFrame();
                overlay.DrawText(12, 12, L"等待 cs2.exe …", RGB(255, 210, 60));
                overlay.EndFrame();
                Sleep(400);
                continue;
            }
            entities = std::make_unique<sdk::EntitySystem>(process);
        }

        const DWORD pid = process.process_id();
        if (!screen_label::sync_overlay_to_cs2(overlay, pid)) {
            overlay.BeginFrame();
            overlay.DrawText(12, 12, L"未找到 CS2 窗口", RGB(255, 120, 80));
            overlay.EndFrame();
            Sleep(200);
            continue;
        }

        color_picker.poll(overlay, box_color, skel_color);

        RECT rc{};
        GetClientRect(overlay.Handle(), &rc);
        const int sw = rc.right - rc.left;
        const int sh = rc.bottom - rc.top;

        static std::vector<sdk::EspTarget> cached_targets;
        static ULONGLONG last_collect = 0;
        const ULONGLONG tick = GetTickCount64();
        if (enabled && entities && (show_box || show_skeleton)) {
            if (const auto fresh_vm = entities->view_matrix()) {
                if (cached_targets.empty() || tick - last_collect >= 32) {
                    cached_targets =
                        entities->collect_enemies(*fresh_vm, sw, sh, show_skeleton, nullptr);
                    last_collect = tick;
                } else {
                    for (sdk::EspTarget& target : cached_targets)
                        sdk::reproject_box(target, *fresh_vm, sw, sh);
                }
            }
        } else if (!enabled) {
            cached_targets.clear();
        }

        std::uint64_t sig = 1469598103934665603ull;
        auto mix = [&](unsigned value) {
            sig ^= value;
            sig *= 1099511628211ull;
        };
        mix(enabled);
        mix(show_box);
        mix(show_skeleton);
        mix(box_color);
        mix(skel_color);
        mix(color_picker.visible());
        auto q = [](float v) { return static_cast<unsigned>(static_cast<int>(v) & ~1); };
        for (const sdk::EspTarget& t : cached_targets) {
            mix(q(t.x));
            mix(q(t.y));
            mix(q(t.w));
            mix(q(t.h));
            mix(static_cast<unsigned>(t.health));
            mix(t.has_c4 ? 1u : 0u);
        }
        static std::uint64_t last_sig = 0;
        if (!color_picker.visible() && sig == last_sig) {
            Sleep(16);
            continue;
        }
        last_sig = sig;

        overlay.BeginFrame();

        wchar_t line0[180]{};
        swprintf_s(line0, L"标注: %s  |  方框:%s  骨骼:%s  |  F7 颜色面板:%s",
                   enabled ? L"开" : L"关", show_box ? L"开" : L"关", show_skeleton ? L"开" : L"关",
                   color_picker.visible() ? L"开" : L"关");
        overlay.DrawText(12, 12, line0, RGB(200, 230, 255));

        wchar_t box_rgb[48]{};
        wchar_t skel_rgb[48]{};
        format_rgb(box_rgb, 48, box_color);
        format_rgb(skel_rgb, 48, skel_color);
        wchar_t line1[220]{};
        swprintf_s(line1, L"方框 %s   骨骼 %s   (F7 面板：预设/自定义)", box_rgb, skel_rgb);
        overlay.DrawText(12, 32, line1, RGB(180, 200, 220));

        if (enabled && !show_box && !show_skeleton) {
            overlay.DrawText(12, 52, L"提示: 方框与骨骼均已关闭，请按 Home 或 PageUp 开启其一",
                             RGB(255, 180, 80));
        }

        if (enabled && entities && (show_box || show_skeleton)) {
            if (!entities->view_matrix() && cached_targets.empty())
                overlay.DrawText(12, 52, L"ViewMatrix 无效 — 检查 offsets", RGB(255, 180, 80));
            for (const sdk::EspTarget& t : cached_targets) {
                if (show_box && t.h >= 2.f && t.w >= 1.f)
                    draw_target_box(overlay, t, box_color);
                if (t.has_c4)
                    overlay.DrawText(static_cast<int>(t.x), static_cast<int>(t.y) - 16, L"C4",
                                     RGB(255, 210, 40));
                if (show_skeleton)
                    draw_target_skeleton(overlay, t, skel_color);
            }
        }

        color_picker.draw(overlay, box_color, skel_color);

        overlay.EndFrame();
        Sleep(16);
    }

    overlay.SetClickThrough(true);
    overlay.Destroy();
    return 0;
}
