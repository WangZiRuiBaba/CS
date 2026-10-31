#pragma once

#include "render/color_palette.hpp"

#include <optional>

namespace render {

enum class ColorPickerTab { Presets, Custom };

struct UiRect {
    int l{};
    int t{};
    int r{};
    int b{};

    [[nodiscard]] bool contains(int x, int y) const {
        return x >= l && x < r && y >= t && y < b;
    }
};

struct CustomColorState {
    float box_h = 0.f;
    float box_s = 1.f;
    float box_v = 1.f;
    float skel_h = 195.f;
    float skel_s = 1.f;
    float skel_v = 1.f;
    ColorPickRow active_target = ColorPickRow::Box;
    ColorPickerTab tab = ColorPickerTab::Presets;

    [[nodiscard]] float active_h() const {
        return active_target == ColorPickRow::Box ? box_h : skel_h;
    }
    [[nodiscard]] float active_s() const {
        return active_target == ColorPickRow::Box ? box_s : skel_s;
    }
    [[nodiscard]] float active_v() const {
        return active_target == ColorPickRow::Box ? box_v : skel_v;
    }

    void set_active_hsv(float h, float s, float v);
    [[nodiscard]] COLORREF active_color() const;
    [[nodiscard]] COLORREF color_for(ColorPickRow row) const;

    void sync_hsv_from_color(ColorPickRow row, COLORREF c);
    void apply_color(ColorPickRow row, COLORREF c);
};

enum class ColorPickerHit {
    None,
    TabPresets,
    TabCustom,
    Swatch,
    TargetBox,
    TargetSkeleton,
    SvPlane,
    HueBar,
};

struct ColorPickerHitResult {
    ColorPickerHit hit = ColorPickerHit::None;
    ColorPickRow swatch_row = ColorPickRow::Box;
    COLORREF swatch_color = 0;
};

struct ColorPickerPanelLayout {
    int panel_l{};
    int panel_t{};
    int panel_r{};
    int panel_b{};
    ColorPickerTab tab = ColorPickerTab::Presets;
    UiRect tab_presets{};
    UiRect tab_custom{};
    ColorPaletteLayout presets{};
    UiRect target_box{};
    UiRect target_skeleton{};
    UiRect preview{};
    UiRect sv_plane{};
    UiRect hue_bar{};
    UiRect rgb_label{};
};

class ColorPickerPanel {
public:
    [[nodiscard]] static ColorPickerPanelLayout layout(int overlay_w, int overlay_h,
                                                       ColorPickerTab tab);

    [[nodiscard]] static ColorPickerHitResult hit_test(const ColorPickerPanelLayout& layout,
                                                       int client_x, int client_y);

    static void update_from_sv(const ColorPickerPanelLayout& layout, int x, int y,
                               CustomColorState& state);
    static void update_from_hue(const ColorPickerPanelLayout& layout, int x,
                                CustomColorState& state);
};

[[nodiscard]] COLORREF hsv_to_colorref(float h, float s, float v);
void colorref_to_hsv(COLORREF c, float& h, float& s, float& v);

} // namespace render
