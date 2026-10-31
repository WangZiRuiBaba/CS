#pragma once

#include "render/color_palette.hpp"
#include "render/color_picker_panel.hpp"
#include "sdk/entity.hpp"

#include <Windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace render {

struct OverlayHud {
    bool esp_enabled = true;
    bool skeleton_enabled = false;
    bool aim_toggled = false;
    bool color_picker_visible = false;
    ColorPickerTab color_picker_tab = ColorPickerTab::Presets;
    CustomColorState color_custom{};
    COLORREF box_color = RGB(255, 55, 55);
    COLORREF skeleton_color = RGB(0, 210, 255);
    std::wstring toast;
    std::chrono::steady_clock::time_point toast_until{};
    // 游戏窗口在前台时屏幕顶部常驻快捷键说明
    std::wstring help_caption;
};

class Overlay {
public:
    Overlay();
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool create();
    void destroy();

    [[nodiscard]] bool is_valid() const { return hwnd_ != nullptr; }
    [[nodiscard]] HWND hwnd() const { return hwnd_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    bool sync_to_game_window(DWORD game_process_id = 0);
    void set_visible(bool visible);
    void set_click_through(bool click_through);
    void render(const std::vector<sdk::EspTarget>& targets, const OverlayHud& hud);

    [[nodiscard]] ColorPickerHitResult hit_color_picker(const OverlayHud& hud, int client_x,
                                                        int client_y) const;

    void pump_messages();

private:
    bool init_d2d();
    void release_d2d();
    bool recreate_target();
    void draw_toast(const OverlayHud& hud);
    void draw_help_caption(const OverlayHud& hud);
    void draw_target_health(const sdk::EspTarget& target);
    void draw_target_skeleton(const sdk::EspTarget& target, ID2D1SolidColorBrush* brush);
    void draw_color_picker(const OverlayHud& hud);
    void ensure_stroke_brush(ID2D1SolidColorBrush*& brush, const D2D1_COLOR_F& color);

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    std::wstring class_name_;
    HWND hwnd_{nullptr};
    HWND game_hwnd_{nullptr};
    int width_{0};
    int height_{0};
    int last_pos_x_{-1};
    int last_pos_y_{-1};
    ULONGLONG last_topmost_ms_{0};
    std::uint64_t last_frame_sig_{0};
    bool visible_{true};

    ID2D1Factory* factory_{nullptr};
    ID2D1HwndRenderTarget* target_{nullptr};
    ID2D1SolidColorBrush* brush_enemy_{nullptr};
    ID2D1SolidColorBrush* brush_box_{nullptr};
    ID2D1SolidColorBrush* brush_skeleton_{nullptr};
    ID2D1SolidColorBrush* brush_swatch_border_{nullptr};
    ID2D1SolidColorBrush* brush_swatch_fill_{nullptr};
    ID2D1SolidColorBrush* brush_hud_text_{nullptr};
    ID2D1SolidColorBrush* brush_hud_panel_{nullptr};
    ID2D1SolidColorBrush* brush_hp_high_{nullptr};
    ID2D1SolidColorBrush* brush_hp_mid_{nullptr};
    ID2D1SolidColorBrush* brush_hp_low_{nullptr};
    IDWriteFactory* dwrite_factory_{nullptr};
    IDWriteTextFormat* text_toast_{nullptr};
    IDWriteTextFormat* text_health_{nullptr};
};

HWND find_cs2_window(DWORD game_process_id = 0);

} // namespace render
