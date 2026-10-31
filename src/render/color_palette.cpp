#include "render/color_palette.hpp"

#include <algorithm>
#include <d2d1.h>

namespace render {

namespace {

constexpr COLORREF kPalette[ColorPaletteUi::kSwatchCount] = {
    RGB(255, 55, 55),
    RGB(255, 120, 40),
    RGB(255, 210, 0),
    RGB(110, 255, 90),
    RGB(0, 210, 255),
    RGB(80, 140, 255),
    RGB(160, 100, 255),
    RGB(255, 120, 255),
    RGB(255, 255, 255),
    RGB(200, 200, 200),
    RGB(120, 120, 120),
    RGB(30, 30, 30),
};

constexpr int kPad = 10;
constexpr int kSwatch = 26;
constexpr int kGap = 6;
constexpr int kRowGap = 28;
constexpr int kHeader = 22;

} // namespace

COLORREF ColorPaletteUi::swatch_color(int index) {
    if (index < 0 || index >= kSwatchCount)
        return RGB(255, 255, 255);
    return kPalette[index];
}

bool ColorPaletteUi::colors_match(COLORREF a, COLORREF b) {
    return GetRValue(a) == GetRValue(b) && GetGValue(a) == GetGValue(b) &&
           GetBValue(a) == GetBValue(b);
}

ColorPaletteLayout ColorPaletteUi::layout(int overlay_w, int overlay_h) {
    ColorPaletteLayout out{};

    const int row_w = kSwatchCount * kSwatch + (kSwatchCount - 1) * kGap;
    const int panel_w = row_w + kPad * 2;
    const int panel_h = kPad * 2 + kHeader + kRowGap + kSwatch + kRowGap + kSwatch;

    out.panel_l = (std::max)(8, (overlay_w - panel_w) / 2);
    out.panel_t = (std::max)(56, overlay_h - panel_h - 24);
    out.panel_r = out.panel_l + panel_w;
    out.panel_b = out.panel_t + panel_h;

    const int row_l = out.panel_l + kPad;
    const int box_row_t = out.panel_t + kPad + kHeader;
    const int skel_row_t = box_row_t + kSwatch + kRowGap;

    for (int i = 0; i < kSwatchCount; ++i) {
        const int x = row_l + i * (kSwatch + kGap);
        out.swatches[static_cast<std::size_t>(i)] = ColorSwatchRect{
            x, box_row_t, x + kSwatch, box_row_t + kSwatch, kPalette[i], ColorPickRow::Box};
        out.swatches[static_cast<std::size_t>(kSwatchCount + i)] = ColorSwatchRect{
            x, skel_row_t, x + kSwatch, skel_row_t + kSwatch, kPalette[i], ColorPickRow::Skeleton};
    }

    return out;
}

bool ColorPaletteUi::hit_test(const ColorPaletteLayout& layout, int client_x, int client_y,
                              ColorPickRow& row, COLORREF& color) {
    for (const ColorSwatchRect& s : layout.swatches) {
        if (s.contains(client_x, client_y)) {
            row = s.row;
            color = s.color;
            return true;
        }
    }
    return false;
}

D2D1_COLOR_F colorref_to_d2d(COLORREF c) {
    return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f, 1.0f);
}

} // namespace render
