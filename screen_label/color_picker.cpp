#include "color_picker.h"

#include <cstdio>

namespace screen_label {

namespace {

bool left_click_edge() {
    static bool prev = false;
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool edge = down && !prev;
    prev = down;
    return edge;
}

void draw_ui_rect(GdiOverlay& overlay, const render::UiRect& r, COLORREF fill, COLORREF border,
                  int border_w) {
    overlay.DrawFilledRect(r.l, r.t, r.r, r.b, fill, border, border_w);
}

} // namespace

COLORREF ColorPickerUi::swatch_color(int index) {
    return render::ColorPaletteUi::swatch_color(index);
}

void ColorPickerUi::set_visible(bool visible) {
    visible_ = visible;
    if (!visible_) {
        drag_sv_ = false;
        drag_hue_ = false;
    }
}

void ColorPickerUi::on_open(COLORREF box_color, COLORREF skeleton_color) {
    custom_.sync_hsv_from_color(render::ColorPickRow::Box, box_color);
    custom_.sync_hsv_from_color(render::ColorPickRow::Skeleton, skeleton_color);
}

void ColorPickerUi::toggle_tab() {
    custom_.tab = custom_.tab == render::ColorPickerTab::Presets ? render::ColorPickerTab::Custom
                                                                 : render::ColorPickerTab::Presets;
}

void ColorPickerUi::poll(GdiOverlay& overlay, COLORREF& box_color, COLORREF& skeleton_color) {
    if (!visible_ || !overlay.Handle())
        return;

    RECT rc{};
    GetClientRect(overlay.Handle(), &rc);
    const int sw = rc.right - rc.left;
    const int sh = rc.bottom - rc.top;
    const render::ColorPickerPanelLayout layout =
        render::ColorPickerPanel::layout(sw, sh, custom_.tab);

    const bool lbtn = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(overlay.Handle(), &pt);

    const auto apply_custom = [&] {
        box_color = custom_.color_for(render::ColorPickRow::Box);
        skeleton_color = custom_.color_for(render::ColorPickRow::Skeleton);
    };

    if (lbtn) {
        if (drag_sv_) {
            render::ColorPickerPanel::update_from_sv(layout, pt.x, pt.y, custom_);
            apply_custom();
            return;
        }
        if (drag_hue_) {
            render::ColorPickerPanel::update_from_hue(layout, pt.x, custom_);
            apply_custom();
            return;
        }
    } else {
        drag_sv_ = false;
        drag_hue_ = false;
    }

    if (!left_click_edge())
        return;

    const render::ColorPickerHitResult hit =
        render::ColorPickerPanel::hit_test(layout, pt.x, pt.y);

    switch (hit.hit) {
    case render::ColorPickerHit::TabPresets:
        custom_.tab = render::ColorPickerTab::Presets;
        break;
    case render::ColorPickerHit::TabCustom:
        custom_.tab = render::ColorPickerTab::Custom;
        on_open(box_color, skeleton_color);
        break;
    case render::ColorPickerHit::Swatch:
        if (hit.swatch_row == render::ColorPickRow::Box)
            box_color = hit.swatch_color;
        else
            skeleton_color = hit.swatch_color;
        custom_.apply_color(hit.swatch_row, hit.swatch_color);
        break;
    case render::ColorPickerHit::TargetBox:
        custom_.active_target = render::ColorPickRow::Box;
        break;
    case render::ColorPickerHit::TargetSkeleton:
        custom_.active_target = render::ColorPickRow::Skeleton;
        break;
    case render::ColorPickerHit::SvPlane:
        drag_sv_ = true;
        render::ColorPickerPanel::update_from_sv(layout, pt.x, pt.y, custom_);
        apply_custom();
        break;
    case render::ColorPickerHit::HueBar:
        drag_hue_ = true;
        render::ColorPickerPanel::update_from_hue(layout, pt.x, custom_);
        apply_custom();
        break;
    default:
        break;
    }
}

void ColorPickerUi::draw(GdiOverlay& overlay, COLORREF box_color, COLORREF skeleton_color) const {
    if (!visible_ || !overlay.Handle())
        return;

    RECT rc{};
    GetClientRect(overlay.Handle(), &rc);
    const int sw = rc.right - rc.left;
    const int sh = rc.bottom - rc.top;
    const render::ColorPickerPanelLayout layout =
        render::ColorPickerPanel::layout(sw, sh, custom_.tab);

    draw_ui_rect(overlay, render::UiRect{layout.panel_l, layout.panel_t, layout.panel_r, layout.panel_b},
                 RGB(18, 22, 32), RGB(90, 110, 150), 2);

    const auto draw_tab = [&](const render::UiRect& r, const wchar_t* label, bool active) {
        draw_ui_rect(overlay, r, active ? RGB(46, 56, 82) : RGB(26, 30, 40),
                     active ? RGB(220, 230, 245) : RGB(80, 90, 110), active ? 2 : 1);
        overlay.DrawText(r.l + 6, r.t + 4, label, RGB(220, 230, 245));
    };

    draw_tab(layout.tab_presets, L"\u9884\u8bbe", custom_.tab == render::ColorPickerTab::Presets);
    draw_tab(layout.tab_custom, L"\u81ea\u5b9a\u4e49", custom_.tab == render::ColorPickerTab::Custom);

    if (custom_.tab == render::ColorPickerTab::Presets) {
        overlay.DrawText(layout.panel_l + 10, layout.panel_t + 34, L"\u65b9\u6846\u989c\u8272 \u2014 \u70b9\u9009\u8272\u5757",
                         RGB(220, 230, 245));
        overlay.DrawText(layout.panel_l + 10, layout.panel_t + 34 + 22 + 26 + 4,
                         L"\u9aa8\u9abc\u989c\u8272 \u2014 \u70b9\u9009\u8272\u5757", RGB(220, 230, 245));

        for (const render::ColorSwatchRect& s : layout.presets.swatches) {
            const bool selected =
                (s.row == render::ColorPickRow::Box &&
                 render::ColorPaletteUi::colors_match(s.color, box_color)) ||
                (s.row == render::ColorPickRow::Skeleton &&
                 render::ColorPaletteUi::colors_match(s.color, skeleton_color));
            overlay.DrawFilledRect(s.l, s.t, s.r, s.b, s.color, selected ? RGB(255, 255, 255) : RGB(60, 60, 70),
                                   selected ? 3 : 1);
        }
    } else {
        const bool edit_box = custom_.active_target == render::ColorPickRow::Box;
        draw_ui_rect(overlay, layout.target_box, edit_box ? box_color : RGB(40, 44, 52),
                     edit_box ? RGB(255, 255, 255) : RGB(70, 70, 80), edit_box ? 2 : 1);
        draw_ui_rect(overlay, layout.target_skeleton, !edit_box ? skeleton_color : RGB(40, 44, 52),
                     !edit_box ? RGB(255, 255, 255) : RGB(70, 70, 80), !edit_box ? 2 : 1);
        overlay.DrawText(layout.target_box.l + 8, layout.target_box.t + 6, L"\u65b9\u6846",
                         RGB(240, 240, 240));
        overlay.DrawText(layout.target_skeleton.l + 8, layout.target_skeleton.t + 6, L"\u9aa8\u9abc",
                         RGB(240, 240, 240));
        draw_ui_rect(overlay, layout.preview, custom_.active_color(), RGB(255, 255, 255), 2);

        constexpr int cols = 40;
        constexpr int rows = 20;
        const int cw = (layout.sv_plane.r - layout.sv_plane.l) / cols;
        const int rh = (layout.sv_plane.b - layout.sv_plane.t) / rows;
        const float hue = custom_.active_h();
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const float s = (col + 0.5f) / cols;
                const float v = 1.f - (row + 0.5f) / rows;
                const COLORREF c = render::hsv_to_colorref(hue, s, v);
                overlay.DrawFilledRect(layout.sv_plane.l + col * cw, layout.sv_plane.t + row * rh,
                                       layout.sv_plane.l + (col + 1) * cw,
                                       layout.sv_plane.t + (row + 1) * rh, c, c, 0);
            }
        }
        overlay.DrawLine(layout.sv_plane.l, layout.sv_plane.t, layout.sv_plane.r, layout.sv_plane.t,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.sv_plane.r, layout.sv_plane.t, layout.sv_plane.r, layout.sv_plane.b,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.sv_plane.r, layout.sv_plane.b, layout.sv_plane.l, layout.sv_plane.b,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.sv_plane.l, layout.sv_plane.b, layout.sv_plane.l, layout.sv_plane.t,
                         RGB(200, 210, 230), 1);

        constexpr int hue_steps = 60;
        const int hw = (layout.hue_bar.r - layout.hue_bar.l) / hue_steps;
        for (int i = 0; i < hue_steps; ++i) {
            const COLORREF c = render::hsv_to_colorref(i * (360.f / hue_steps), 1.f, 1.f);
            overlay.DrawFilledRect(layout.hue_bar.l + i * hw, layout.hue_bar.t,
                                   layout.hue_bar.l + (i + 1) * hw, layout.hue_bar.b, c, c, 0);
        }
        overlay.DrawLine(layout.hue_bar.l, layout.hue_bar.t, layout.hue_bar.r, layout.hue_bar.t,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.hue_bar.r, layout.hue_bar.t, layout.hue_bar.r, layout.hue_bar.b,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.hue_bar.r, layout.hue_bar.b, layout.hue_bar.l, layout.hue_bar.b,
                         RGB(200, 210, 230), 1);
        overlay.DrawLine(layout.hue_bar.l, layout.hue_bar.b, layout.hue_bar.l, layout.hue_bar.t,
                         RGB(200, 210, 230), 1);

        wchar_t rgb[80]{};
        const COLORREF ac = custom_.active_color();
        swprintf_s(rgb, L"RGB %u,%u,%u \u00b7 \u62d6\u62fd\u8272\u5761/\u8272\u6761", GetRValue(ac),
                   GetGValue(ac), GetBValue(ac));
        overlay.DrawText(layout.rgb_label.l, layout.rgb_label.t, rgb, RGB(180, 200, 220));
    }

    overlay.DrawText(layout.panel_l + 10, layout.panel_b - 18,
                     L"F7 \u5173\u95ed \u00b7 Tab \u5207\u6362\u9884\u8bbe/\u81ea\u5b9a\u4e49",
                     RGB(150, 170, 200));
}

} // namespace screen_label
