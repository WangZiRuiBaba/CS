#pragma once

#include "overlay.h"

#include "render/color_palette.hpp"
#include "render/color_picker_panel.hpp"

namespace screen_label {

class ColorPickerUi {
public:
    static constexpr int kSwatchCount = render::ColorPaletteUi::kSwatchCount;

    [[nodiscard]] static COLORREF swatch_color(int index);

    void set_visible(bool visible);
    [[nodiscard]] bool visible() const { return visible_; }
    void on_open(COLORREF box_color, COLORREF skeleton_color);

    void toggle_tab();
    void poll(GdiOverlay& overlay, COLORREF& box_color, COLORREF& skeleton_color);

    void draw(GdiOverlay& overlay, COLORREF box_color, COLORREF skeleton_color) const;

private:
    bool visible_ = false;
    render::CustomColorState custom_{};
    bool drag_sv_ = false;
    bool drag_hue_ = false;
};

} // namespace screen_label
