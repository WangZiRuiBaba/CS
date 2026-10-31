#pragma once

#include <Windows.h>

#include <d2d1.h>

#include <array>
#include <cstddef>

namespace render {

enum class ColorPickRow { Box, Skeleton };

struct ColorSwatchRect {
    int l{};
    int t{};
    int r{};
    int b{};
    COLORREF color{};
    ColorPickRow row{};

    [[nodiscard]] bool contains(int x, int y) const {
        return x >= l && x < r && y >= t && y < b;
    }
};

struct ColorPaletteLayout {
    int panel_l{};
    int panel_t{};
    int panel_r{};
    int panel_b{};
    std::array<ColorSwatchRect, 24> swatches{};
};

class ColorPaletteUi {
public:
    static constexpr int kSwatchCount = 12;

    [[nodiscard]] static COLORREF swatch_color(int index);
    [[nodiscard]] static bool colors_match(COLORREF a, COLORREF b);
    [[nodiscard]] static ColorPaletteLayout layout(int overlay_w, int overlay_h);

    [[nodiscard]] static bool hit_test(const ColorPaletteLayout& layout, int client_x, int client_y,
                                       ColorPickRow& row, COLORREF& color);
};

[[nodiscard]] D2D1_COLOR_F colorref_to_d2d(COLORREF c);

} // namespace render
