#include "render/overlay.hpp"

#include "stealth/stealth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <dwrite.h>

namespace render {

namespace {

const D2D1_COLOR_F kColorEnemy = {1.0f, 0.15f, 0.15f, 1.0f};

float snap_draw_coord(const float v) { return std::floor(v) + 0.5f; }
const D2D1_COLOR_F kColorClear = {1.0f, 0.0f, 1.0f, 1.0f};
constexpr int kSizeHysteresisPx = 2;
constexpr int kPosHysteresisPx = 1;
bool toast_visible(const OverlayHud& hud) {
    if (hud.toast.empty())
        return false;
    return std::chrono::steady_clock::now() < hud.toast_until;
}

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

} // namespace

bool hwnd_belongs_to_process(const HWND hwnd, const DWORD process_id) {
    if (!hwnd || process_id == 0)
        return false;
    DWORD window_pid = 0;
    GetWindowThreadProcessId(hwnd, &window_pid);
    return window_pid == process_id;
}

HWND find_cs2_window(const DWORD game_process_id) {
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

Overlay::Overlay() { class_name_ = stealth::random_overlay_class_name(); }

Overlay::~Overlay() { destroy(); }

bool Overlay::create() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = class_name_.c_str();
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        class_name_.c_str(), L"", WS_POPUP, 0, 0, 800, 600, nullptr, nullptr, wc.hInstance,
        nullptr);

    if (!hwnd_)
        return false;

    SetLayeredWindowAttributes(hwnd_, RGB(255, 0, 255), 0, LWA_COLORKEY);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    return init_d2d();
}

void Overlay::destroy() {
    release_d2d();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (!class_name_.empty())
        UnregisterClassW(class_name_.c_str(), GetModuleHandleW(nullptr));
}

void Overlay::set_visible(const bool visible) {
    visible_ = visible;
    if (!hwnd_)
        return;
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

bool Overlay::init_d2d() {
    release_d2d();

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_)))
        return false;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&dwrite_factory_))))
        return false;

    if (FAILED(dwrite_factory_->CreateTextFormat(
            L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"zh-cn", &text_toast_)))
        return false;
    text_toast_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    text_toast_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    if (FAILED(dwrite_factory_->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &text_health_)))
        return false;
    text_health_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    text_health_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    return recreate_target();
}

void Overlay::release_d2d() {
    if (text_health_) {
        text_health_->Release();
        text_health_ = nullptr;
    }
    if (text_toast_) {
        text_toast_->Release();
        text_toast_ = nullptr;
    }
    if (dwrite_factory_) {
        dwrite_factory_->Release();
        dwrite_factory_ = nullptr;
    }
    if (brush_hp_low_) {
        brush_hp_low_->Release();
        brush_hp_low_ = nullptr;
    }
    if (brush_hp_mid_) {
        brush_hp_mid_->Release();
        brush_hp_mid_ = nullptr;
    }
    if (brush_hp_high_) {
        brush_hp_high_->Release();
        brush_hp_high_ = nullptr;
    }
    if (brush_hud_panel_) {
        brush_hud_panel_->Release();
        brush_hud_panel_ = nullptr;
    }
    if (brush_hud_text_) {
        brush_hud_text_->Release();
        brush_hud_text_ = nullptr;
    }
    if (brush_swatch_fill_) {
        brush_swatch_fill_->Release();
        brush_swatch_fill_ = nullptr;
    }
    if (brush_swatch_border_) {
        brush_swatch_border_->Release();
        brush_swatch_border_ = nullptr;
    }
    if (brush_skeleton_) {
        brush_skeleton_->Release();
        brush_skeleton_ = nullptr;
    }
    if (brush_box_) {
        brush_box_->Release();
        brush_box_ = nullptr;
    }
    if (brush_enemy_) {
        brush_enemy_->Release();
        brush_enemy_ = nullptr;
    }
    if (target_) {
        target_->Release();
        target_ = nullptr;
    }
    if (factory_) {
        factory_->Release();
        factory_ = nullptr;
    }
}

bool Overlay::recreate_target() {
    if (!factory_ || !hwnd_)
        return false;

    if (target_) {
        target_->Release();
        target_ = nullptr;
    }
    if (brush_swatch_fill_) {
        brush_swatch_fill_->Release();
        brush_swatch_fill_ = nullptr;
    }
    if (brush_swatch_border_) {
        brush_swatch_border_->Release();
        brush_swatch_border_ = nullptr;
    }
    if (brush_skeleton_) {
        brush_skeleton_->Release();
        brush_skeleton_ = nullptr;
    }
    if (brush_box_) {
        brush_box_->Release();
        brush_box_ = nullptr;
    }
    if (brush_enemy_) {
        brush_enemy_->Release();
        brush_enemy_ = nullptr;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const D2D1_SIZE_U size = {static_cast<UINT32>(rc.right - rc.left),
                              static_cast<UINT32>(rc.bottom - rc.top)};

    // Software target keeps the game on its own flip presentation. A hardware
    // overlay swapchain drops CS2 out of independent flip and costs frames.
    const auto rt_props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    if (FAILED(factory_->CreateHwndRenderTarget(
            rt_props,
            D2D1::HwndRenderTargetProperties(hwnd_, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
            &target_)))
        return false;

    target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target_->CreateSolidColorBrush(kColorEnemy, &brush_enemy_);
    target_->CreateSolidColorBrush(kColorEnemy, &brush_box_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.82f, 1.0f, 1.0f), &brush_skeleton_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.24f, 0.24f, 0.27f, 1.0f), &brush_swatch_border_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.07f, 0.09f, 0.13f, 1.0f), &brush_swatch_fill_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.94f, 0.98f, 0.95f), &brush_hud_text_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.05f, 0.06f, 0.08f, 1.0f), &brush_hud_panel_);
    target_->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.98f, 0.55f, 1.0f), &brush_hp_high_);
    target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.88f, 0.35f, 1.0f), &brush_hp_mid_);
    target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.42f, 0.42f, 1.0f), &brush_hp_low_);
    return target_ && brush_enemy_ && brush_box_ && brush_skeleton_ && brush_swatch_border_ &&
           brush_swatch_fill_ && brush_hud_text_ && brush_hud_panel_ && brush_hp_high_ &&
           brush_hp_mid_ && brush_hp_low_ && text_toast_ && text_health_;
}

void Overlay::set_click_through(const bool click_through) {
    if (!hwnd_)
        return;
    LONG ex = GetWindowLongW(hwnd_, GWL_EXSTYLE);
    if (click_through)
        ex |= WS_EX_TRANSPARENT;
    else
        ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongW(hwnd_, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void Overlay::ensure_stroke_brush(ID2D1SolidColorBrush*& brush, const D2D1_COLOR_F& color) {
    if (!target_)
        return;
    if (!brush) {
        target_->CreateSolidColorBrush(color, &brush);
        return;
    }
    brush->SetColor(color);
}

ColorPickerHitResult Overlay::hit_color_picker(const OverlayHud& hud, int client_x,
                                               int client_y) const {
    if (!hud.color_picker_visible)
        return {};
    const ColorPickerPanelLayout layout =
        ColorPickerPanel::layout(width_, height_, hud.color_picker_tab);
    return ColorPickerPanel::hit_test(layout, client_x, client_y);
}

bool Overlay::sync_to_game_window(const DWORD game_process_id) {
    if (!visible_)
        return false;

    if (!game_hwnd_ || !IsWindow(game_hwnd_) ||
        !hwnd_belongs_to_process(game_hwnd_, game_process_id)) {
        game_hwnd_ = find_cs2_window(game_process_id);
    }

    if (!game_hwnd_ || !IsWindow(game_hwnd_))
        return false;

    RECT client{};
    if (!GetClientRect(game_hwnd_, &client))
        return false;

    POINT top_left{client.left, client.top};
    ClientToScreen(game_hwnd_, &top_left);

    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    if (w <= 0 || h <= 0)
        return false;

    const bool size_changed =
        width_ <= 0 || height_ <= 0 || std::abs(w - width_) > kSizeHysteresisPx ||
        std::abs(h - height_) > kSizeHysteresisPx;
    const bool pos_changed = last_pos_x_ < 0 || last_pos_y_ < 0 ||
                             std::abs(top_left.x - last_pos_x_) > kPosHysteresisPx ||
                             std::abs(top_left.y - last_pos_y_) > kPosHysteresisPx;

    if (size_changed) {
        width_ = w;
        height_ = h;
        SetWindowPos(hwnd_, HWND_TOPMOST, top_left.x, top_left.y, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);

        const D2D1_SIZE_U size = {static_cast<UINT32>(w), static_cast<UINT32>(h)};
        if (!target_ || FAILED(target_->Resize(size)))
            recreate_target();
    } else if (pos_changed) {
        SetWindowPos(hwnd_, HWND_TOPMOST, top_left.x, top_left.y, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
    } else if (GetTickCount64() - last_topmost_ms_ >= 500) {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        last_topmost_ms_ = GetTickCount64();
    }

    last_pos_x_ = top_left.x;
    last_pos_y_ = top_left.y;
    if (size_changed || pos_changed)
        last_topmost_ms_ = GetTickCount64();

    return true;
}

void Overlay::draw_help_caption(const OverlayHud& hud) {
    if (!target_ || !text_health_ || !brush_hud_text_ || !brush_hud_panel_ || hud.help_caption.empty())
        return;

    const float panel_w = 580.0f;
    const float panel_h = 22.0f;
    const float top = 8.0f;
    const float cx = width_ * 0.5f;
    const D2D1_RECT_F panel = {cx - panel_w * 0.5f, top, cx + panel_w * 0.5f, top + panel_h};
    target_->FillRectangle(panel, brush_hud_panel_);

    const D2D1_RECT_F text = {panel.left + 6.0f, panel.top + 2.0f, panel.right - 6.0f,
                              panel.bottom - 2.0f};
    target_->DrawText(hud.help_caption.c_str(), static_cast<UINT32>(hud.help_caption.size()),
                      text_health_, text, brush_hud_text_);
}

void Overlay::draw_toast(const OverlayHud& hud) {
    if (!target_ || !text_toast_ || !brush_hud_text_ || !brush_hud_panel_ || !toast_visible(hud))
        return;

    const float toast_w = 320.0f;
    const float toast_h = 46.0f;
    const float margin = 36.0f;
    const float cx = width_ * 0.5f;
    const float bottom = static_cast<float>(height_ > 0 ? height_ : 600);
    const D2D1_RECT_F toast_panel = {cx - toast_w * 0.5f, bottom - margin - toast_h,
                                     cx + toast_w * 0.5f, bottom - margin};
    target_->FillRectangle(toast_panel, brush_hud_panel_);

    const D2D1_RECT_F toast_rect = {toast_panel.left, toast_panel.top + 4.0f, toast_panel.right,
                                    toast_panel.bottom - 4.0f};
    target_->DrawText(hud.toast.c_str(), static_cast<UINT32>(hud.toast.size()), text_toast_,
                      toast_rect, brush_hud_text_);
}

void Overlay::draw_target_skeleton(const sdk::EspTarget& t, ID2D1SolidColorBrush* brush) {
    if (!target_ || !brush)
        return;

    for (const auto& line : t.skeleton) {
        target_->DrawLine({snap_draw_coord(line.x1), snap_draw_coord(line.y1)},
                          {snap_draw_coord(line.x2), snap_draw_coord(line.y2)}, brush, 1.0f);
    }

    float hx = t.x + t.w * 0.5f;
    float hy = t.y + t.h * 0.08f;
    if (t.skeleton_head) {
        hx = t.skeleton_head->x;
        hy = t.skeleton_head->y;
    }
    const float radius = (std::max)(2.5f, (std::min)(t.h * 0.045f, 7.0f));
    const D2D1_ELLIPSE head = {D2D1::Point2F(snap_draw_coord(hx), snap_draw_coord(hy)), radius,
                               radius};
    target_->DrawEllipse(head, brush, 1.0f);
}

void Overlay::draw_color_picker(const OverlayHud& hud) {
    if (!target_ || !text_health_ || !brush_hud_text_ || !brush_hud_panel_ || !brush_swatch_border_ ||
        !brush_swatch_fill_ || !hud.color_picker_visible)
        return;

    const ColorPickerPanelLayout layout =
        ColorPickerPanel::layout(width_, height_, hud.color_picker_tab);

    const D2D1_RECT_F panel = {static_cast<float>(layout.panel_l), static_cast<float>(layout.panel_t),
                               static_cast<float>(layout.panel_r), static_cast<float>(layout.panel_b)};
    target_->FillRectangle(panel, brush_hud_panel_);
    target_->DrawRectangle(panel, brush_hud_text_, 1.5f);

    const auto draw_ui_rect = [&](const UiRect& r, const D2D1_COLOR_F& fill, float border_w) {
        const D2D1_RECT_F rf = {static_cast<float>(r.l), static_cast<float>(r.t),
                                static_cast<float>(r.r), static_cast<float>(r.b)};
        ensure_stroke_brush(brush_swatch_fill_, fill);
        target_->FillRectangle(rf, brush_swatch_fill_);
        target_->DrawRectangle(rf, brush_hud_text_, border_w);
    };

    const auto draw_tab = [&](const UiRect& r, const wchar_t* label, bool active) {
        draw_ui_rect(r, active ? D2D1::ColorF(0.18f, 0.22f, 0.32f, 1.f)
                               : D2D1::ColorF(0.10f, 0.12f, 0.16f, 1.f),
                     active ? 2.f : 1.f);
        const D2D1_RECT_F tr = {static_cast<float>(r.l) + 4.f, static_cast<float>(r.t) + 2.f,
                                static_cast<float>(r.r) - 4.f, static_cast<float>(r.b) - 2.f};
        target_->DrawText(label, static_cast<UINT32>(lstrlenW(label)), text_health_, tr,
                          brush_hud_text_);
    };

    draw_tab(layout.tab_presets, L"\u9884\u8bbe", hud.color_picker_tab == ColorPickerTab::Presets);
    draw_tab(layout.tab_custom, L"\u81ea\u5b9a\u4e49",
             hud.color_picker_tab == ColorPickerTab::Custom);

    const auto draw_label = [&](float top, const wchar_t* text) {
        const D2D1_RECT_F rect = {panel.left + 10.0f, top, panel.right - 10.0f, top + 18.0f};
        target_->DrawText(text, static_cast<UINT32>(lstrlenW(text)), text_health_, rect,
                          brush_hud_text_);
    };

    if (hud.color_picker_tab == ColorPickerTab::Presets) {
        draw_label(panel.top + 34.0f, L"\u65b9\u6846\u989c\u8272 \u2014 \u70b9\u9009\u8272\u5757");
        draw_label(panel.top + 34.0f + 22.0f + 26.0f + 4.0f,
                   L"\u9aa8\u9abc\u989c\u8272 \u2014 \u70b9\u9009\u8272\u5757");

        for (const ColorSwatchRect& s : layout.presets.swatches) {
            const bool selected =
                (s.row == ColorPickRow::Box && ColorPaletteUi::colors_match(s.color, hud.box_color)) ||
                (s.row == ColorPickRow::Skeleton &&
                 ColorPaletteUi::colors_match(s.color, hud.skeleton_color));

            const D2D1_RECT_F sw = {static_cast<float>(s.l), static_cast<float>(s.t),
                                    static_cast<float>(s.r), static_cast<float>(s.b)};
            ensure_stroke_brush(brush_swatch_fill_, colorref_to_d2d(s.color));
            target_->FillRectangle(sw, brush_swatch_fill_);
            target_->DrawRectangle(sw, selected ? brush_hud_text_ : brush_swatch_border_,
                                  selected ? 3.0f : 1.0f);
        }
    } else {
        const bool edit_box = hud.color_custom.active_target == ColorPickRow::Box;
        draw_ui_rect(layout.target_box,
                     colorref_to_d2d(edit_box ? hud.box_color : RGB(40, 44, 52)), edit_box ? 2.5f : 1.f);
        draw_ui_rect(layout.target_skeleton,
                     colorref_to_d2d(!edit_box ? hud.skeleton_color : RGB(40, 44, 52)),
                     !edit_box ? 2.5f : 1.f);

        const D2D1_RECT_F box_label = {static_cast<float>(layout.target_box.l) + 4.f,
                                       static_cast<float>(layout.target_box.t) + 4.f,
                                       static_cast<float>(layout.target_box.r) - 4.f,
                                       static_cast<float>(layout.target_box.b) - 4.f};
        target_->DrawText(L"\u65b9\u6846", 2, text_health_, box_label, brush_hud_text_);
        const D2D1_RECT_F sk_label = {static_cast<float>(layout.target_skeleton.l) + 4.f,
                                      static_cast<float>(layout.target_skeleton.t) + 4.f,
                                      static_cast<float>(layout.target_skeleton.r) - 4.f,
                                      static_cast<float>(layout.target_skeleton.b) - 4.f};
        target_->DrawText(L"\u9aa8\u9abc", 2, text_health_, sk_label, brush_hud_text_);

        draw_ui_rect(layout.preview, colorref_to_d2d(hud.color_custom.active_color()), 2.f);

        const int cols = 40;
        const int rows = 20;
        const float cw = static_cast<float>(layout.sv_plane.r - layout.sv_plane.l) / cols;
        const float rh = static_cast<float>(layout.sv_plane.b - layout.sv_plane.t) / rows;
        const float hue = hud.color_custom.active_h();
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const float s = (col + 0.5f) / cols;
                const float v = 1.f - (row + 0.5f) / rows;
                const D2D1_RECT_F cell = {
                    layout.sv_plane.l + col * cw, layout.sv_plane.t + row * rh,
                    layout.sv_plane.l + (col + 1) * cw, layout.sv_plane.t + (row + 1) * rh};
                ensure_stroke_brush(brush_swatch_fill_, colorref_to_d2d(hsv_to_colorref(hue, s, v)));
                target_->FillRectangle(cell, brush_swatch_fill_);
            }
        }
        target_->DrawRectangle(D2D1::RectF(static_cast<float>(layout.sv_plane.l),
                                            static_cast<float>(layout.sv_plane.t),
                                            static_cast<float>(layout.sv_plane.r),
                                            static_cast<float>(layout.sv_plane.b)),
                               brush_hud_text_, 1.5f);

        const int hue_steps = 60;
        const float hw = static_cast<float>(layout.hue_bar.r - layout.hue_bar.l) / hue_steps;
        for (int i = 0; i < hue_steps; ++i) {
            const D2D1_RECT_F seg = {layout.hue_bar.l + i * hw, static_cast<float>(layout.hue_bar.t),
                                     layout.hue_bar.l + (i + 1) * hw,
                                     static_cast<float>(layout.hue_bar.b)};
            ensure_stroke_brush(brush_swatch_fill_,
                                colorref_to_d2d(hsv_to_colorref(i * (360.f / hue_steps), 1.f, 1.f)));
            target_->FillRectangle(seg, brush_swatch_fill_);
        }
        target_->DrawRectangle(D2D1::RectF(static_cast<float>(layout.hue_bar.l),
                                            static_cast<float>(layout.hue_bar.t),
                                            static_cast<float>(layout.hue_bar.r),
                                            static_cast<float>(layout.hue_bar.b)),
                               brush_hud_text_, 1.5f);

        wchar_t rgb[64]{};
        const COLORREF ac = hud.color_custom.active_color();
        swprintf_s(rgb, L"RGB %u,%u,%u \u00b7 \u62d6\u62fd\u8272\u5761/\u8272\u6761",
                   GetRValue(ac), GetGValue(ac), GetBValue(ac));
        const D2D1_RECT_F rgb_rect = {static_cast<float>(layout.rgb_label.l),
                                      static_cast<float>(layout.rgb_label.t),
                                      static_cast<float>(layout.rgb_label.r),
                                      static_cast<float>(layout.rgb_label.b)};
        target_->DrawText(rgb, static_cast<UINT32>(lstrlenW(rgb)), text_health_, rgb_rect,
                          brush_hud_text_);
    }

    draw_label(panel.bottom - 18.0f,
               L"F7 \u5173\u95ed \u00b7 Tab \u5207\u6362\u9884\u8bbe/\u81ea\u5b9a\u4e49");
}

void Overlay::draw_target_health(const sdk::EspTarget& t) {
    if (!target_ || !text_health_ || !brush_hp_high_ || !brush_hp_mid_ || !brush_hp_low_)
        return;

    const std::wstring label = std::to_wstring(t.health);

    ID2D1SolidColorBrush* brush = brush_hp_high_;
    if (t.health <= 30)
        brush = brush_hp_low_;
    else if (t.health <= 60)
        brush = brush_hp_mid_;

    // Left of box, mid-height — avoids covering head / crosshair.
    const float label_top = t.y + t.h * 0.38f;
    const D2D1_RECT_F text_rect = {t.x - 30.0f, label_top, t.x - 3.0f, label_top + 16.0f};
    target_->DrawText(label.c_str(), static_cast<UINT32>(label.size()), text_health_, text_rect,
                      brush);
}

namespace {

std::uint64_t frame_signature(const std::vector<sdk::EspTarget>& targets, const OverlayHud& hud) {
    std::uint64_t sig = 1469598103934665603ull;
    auto mix = [&](int value) {
        sig ^= static_cast<std::uint64_t>(static_cast<unsigned>(value));
        sig *= 1099511628211ull;
    };
    mix(hud.esp_enabled);
    mix(hud.skeleton_enabled);
    mix(static_cast<int>(hud.box_color));
    mix(static_cast<int>(hud.skeleton_color));
    mix(static_cast<int>(targets.size()));
    // 2px grid: sub-pixel view-matrix noise should not repaint the full overlay.
    auto q = [](float v) { return static_cast<int>(v) & ~1; };
    for (const auto& t : targets) {
        mix(q(t.x));
        mix(q(t.y));
        mix(q(t.w));
        mix(q(t.h));
        mix(t.health);
        mix(t.has_c4 ? 1 : 0);
    }
    return sig;
}

} // namespace

void Overlay::render(const std::vector<sdk::EspTarget>& targets, const OverlayHud& hud) {
    if (!visible_ || !target_ || !brush_box_ || !brush_skeleton_)
        return;

    if (!hud.color_picker_visible && !toast_visible(hud)) {
        const std::uint64_t sig = frame_signature(targets, hud);
        if (sig == last_frame_sig_)
            return;
        last_frame_sig_ = sig;
    } else {
        last_frame_sig_ = 0;
    }

    ensure_stroke_brush(brush_box_, colorref_to_d2d(hud.box_color));
    ensure_stroke_brush(brush_skeleton_, colorref_to_d2d(hud.skeleton_color));

    target_->BeginDraw();
    target_->Clear(kColorClear);

    for (const auto& t : targets) {
        if (hud.esp_enabled && t.h >= 2.f && t.w >= 1.f) {
            const float x1 = snap_draw_coord(t.x);
            const float y1 = snap_draw_coord(t.y);
            const float x2 = snap_draw_coord(t.x + t.w);
            const float y2 = snap_draw_coord(t.y + t.h);
            const D2D1_RECT_F rect = {x1, y1, x2, y2};
            target_->DrawRectangle(rect, brush_box_, 1.5f);
            draw_target_health(t);
        }
        if (t.has_c4 && t.h >= 2.f && text_health_ && brush_hp_mid_) {
            const wchar_t* c4 = L"C4";
            const float label_x = snap_draw_coord(t.x);
            const float label_y = snap_draw_coord(t.y) - 16.0f;
            const D2D1_RECT_F c4_rect = {label_x, label_y, label_x + 36.0f, label_y + 16.0f};
            target_->DrawText(c4, 2, text_health_, c4_rect, brush_hp_mid_);
        }
        if (hud.skeleton_enabled)
            draw_target_skeleton(t, brush_skeleton_);
    }

    draw_help_caption(hud);
    draw_color_picker(hud);
    draw_toast(hud);

    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
        recreate_target();
}

void Overlay::pump_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

LRESULT CALLBACK Overlay::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
    case WM_CLOSE:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace render
