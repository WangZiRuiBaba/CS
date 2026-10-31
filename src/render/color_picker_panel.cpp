#include "render/color_picker_panel.hpp"

#include <algorithm>
#include <cmath>

namespace render {

namespace {

constexpr int kPad = 10;
constexpr int kTabH = 24;
constexpr int kTabW = 72;

UiRect make_rect(int l, int t, int w, int h) { return UiRect{l, t, l + w, t + h}; }

float clamp01(float v) { return (std::max)(0.f, (std::min)(1.f, v)); }

} // namespace

COLORREF hsv_to_colorref(float h, float s, float v) {
    h = std::fmod(h, 360.f);
    if (h < 0.f)
        h += 360.f;
    s = clamp01(s);
    v = clamp01(v);

    const float c = v * s;
    const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    const float m = v - c;

    float rp = 0.f, gp = 0.f, bp = 0.f;
    if (h < 60.f) {
        rp = c;
        gp = x;
    } else if (h < 120.f) {
        rp = x;
        gp = c;
    } else if (h < 180.f) {
        gp = c;
        bp = x;
    } else if (h < 240.f) {
        gp = x;
        bp = c;
    } else if (h < 300.f) {
        rp = x;
        bp = c;
    } else {
        rp = c;
        bp = x;
    }

    const int r = static_cast<int>((rp + m) * 255.f + 0.5f);
    const int g = static_cast<int>((gp + m) * 255.f + 0.5f);
    const int b = static_cast<int>((bp + m) * 255.f + 0.5f);
    return RGB((std::min)(255, r), (std::min)(255, g), (std::min)(255, b));
}

void colorref_to_hsv(COLORREF c, float& h, float& s, float& v) {
    const float r = GetRValue(c) / 255.f;
    const float g = GetGValue(c) / 255.f;
    const float b = GetBValue(c) / 255.f;

    const float maxc = (std::max)(r, (std::max)(g, b));
    const float minc = (std::min)(r, (std::min)(g, b));
    const float delta = maxc - minc;

    v = maxc;
    if (maxc <= 1e-5f) {
        h = 0.f;
        s = 0.f;
        return;
    }

    s = delta / maxc;
    if (delta <= 1e-5f) {
        h = 0.f;
        return;
    }

    if (maxc == r) {
        h = 60.f * (std::fmod((g - b) / delta, 6.f));
    } else if (maxc == g) {
        h = 60.f * (((b - r) / delta) + 2.f);
    } else {
        h = 60.f * (((r - g) / delta) + 4.f);
    }
    if (h < 0.f)
        h += 360.f;
}

void CustomColorState::set_active_hsv(float h, float s, float v) {
    if (active_target == ColorPickRow::Box) {
        box_h = h;
        box_s = s;
        box_v = v;
    } else {
        skel_h = h;
        skel_s = s;
        skel_v = v;
    }
}

COLORREF CustomColorState::active_color() const { return hsv_to_colorref(active_h(), active_s(), active_v()); }

COLORREF CustomColorState::color_for(const ColorPickRow row) const {
    if (row == ColorPickRow::Box)
        return hsv_to_colorref(box_h, box_s, box_v);
    return hsv_to_colorref(skel_h, skel_s, skel_v);
}

void CustomColorState::sync_hsv_from_color(const ColorPickRow row, COLORREF c) {
    float h = 0.f, s = 0.f, v = 0.f;
    colorref_to_hsv(c, h, s, v);
    if (row == ColorPickRow::Box) {
        box_h = h;
        box_s = s;
        box_v = v;
    } else {
        skel_h = h;
        skel_s = s;
        skel_v = v;
    }
}

void CustomColorState::apply_color(const ColorPickRow row, COLORREF c) {
    sync_hsv_from_color(row, c);
}

ColorPickerPanelLayout ColorPickerPanel::layout(int overlay_w, int overlay_h, ColorPickerTab tab) {
    ColorPickerPanelLayout out{};
    out.tab = tab;

    const ColorPaletteLayout preset = ColorPaletteUi::layout(overlay_w, overlay_h);

    if (tab == ColorPickerTab::Presets) {
        out.panel_l = preset.panel_l;
        out.panel_t = preset.panel_t - kTabH - 4;
        out.panel_r = preset.panel_r;
        out.panel_b = preset.panel_b;
        out.presets = preset;
    } else {
        const int inner_w = 280;
        const int inner_h = 200;
        const int panel_w = inner_w + kPad * 2;
        const int panel_h = inner_h + kPad * 2 + kTabH + 4;
        out.panel_l = (std::max)(8, (overlay_w - panel_w) / 2);
        out.panel_t = (std::max)(56, overlay_h - panel_h - 24);
        out.panel_r = out.panel_l + panel_w;
        out.panel_b = out.panel_t + panel_h;

        const int content_t = out.panel_t + kPad + kTabH + 4;
        out.target_box = make_rect(out.panel_l + kPad, content_t, 88, 28);
        out.target_skeleton = make_rect(out.panel_l + kPad + 96, content_t, 88, 28);
        out.preview = make_rect(out.panel_r - kPad - 44, content_t, 44, 28);
        out.sv_plane = make_rect(out.panel_l + kPad, content_t + 36, inner_w, 120);
        out.hue_bar = make_rect(out.panel_l + kPad, content_t + 164, inner_w, 18);
        out.rgb_label = make_rect(out.panel_l + kPad, content_t + 186, inner_w, 16);
    }

    out.tab_presets = make_rect(out.panel_l + kPad, out.panel_t + kPad, kTabW, kTabH);
    out.tab_custom = make_rect(out.panel_l + kPad + kTabW + 6, out.panel_t + kPad, kTabW, kTabH);

    if (tab == ColorPickerTab::Presets) {
        const int dy = kTabH + 4;
        out.presets.panel_t += dy;
        out.presets.panel_b += dy;
        for (ColorSwatchRect& s : out.presets.swatches) {
            s.t += dy;
            s.b += dy;
        }
    }

    return out;
}

ColorPickerHitResult ColorPickerPanel::hit_test(const ColorPickerPanelLayout& layout, int client_x,
                                                int client_y) {
    ColorPickerHitResult result{};

    if (layout.tab_presets.contains(client_x, client_y)) {
        result.hit = ColorPickerHit::TabPresets;
        return result;
    }
    if (layout.tab_custom.contains(client_x, client_y)) {
        result.hit = ColorPickerHit::TabCustom;
        return result;
    }

    if (layout.tab == ColorPickerTab::Custom) {
        if (layout.target_box.contains(client_x, client_y)) {
            result.hit = ColorPickerHit::TargetBox;
            return result;
        }
        if (layout.target_skeleton.contains(client_x, client_y)) {
            result.hit = ColorPickerHit::TargetSkeleton;
            return result;
        }
        if (layout.sv_plane.contains(client_x, client_y)) {
            result.hit = ColorPickerHit::SvPlane;
            return result;
        }
        if (layout.hue_bar.contains(client_x, client_y)) {
            result.hit = ColorPickerHit::HueBar;
            return result;
        }
        return result;
    }

    ColorPickRow row{};
    COLORREF color{};
    if (ColorPaletteUi::hit_test(layout.presets, client_x, client_y, row, color)) {
        result.hit = ColorPickerHit::Swatch;
        result.swatch_row = row;
        result.swatch_color = color;
    }
    return result;
}

void ColorPickerPanel::update_from_sv(const ColorPickerPanelLayout& layout, int x, int y,
                                      CustomColorState& state) {
    if (!layout.sv_plane.contains(x, y))
        return;

    const float w = static_cast<float>(layout.sv_plane.r - layout.sv_plane.l);
    const float h = static_cast<float>(layout.sv_plane.b - layout.sv_plane.t);
    const float s = clamp01((x - layout.sv_plane.l) / w);
    const float v = clamp01(1.f - (y - layout.sv_plane.t) / h);
    state.set_active_hsv(state.active_h(), s, v);
}

void ColorPickerPanel::update_from_hue(const ColorPickerPanelLayout& layout, int x,
                                       CustomColorState& state) {
    if (!layout.hue_bar.contains(x, layout.hue_bar.t + 1))
        return;

    const float w = static_cast<float>(layout.hue_bar.r - layout.hue_bar.l);
    const float t = clamp01((x - layout.hue_bar.l) / w);
    state.set_active_hsv(t * 360.f, state.active_s(), state.active_v());
}

} // namespace render
