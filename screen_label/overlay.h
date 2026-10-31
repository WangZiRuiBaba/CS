#pragma once
#include <windows.h>
#include <vector>

struct Box { int l, t, r, b; };

class GdiOverlay {
public:
    bool Create();
    void Destroy();

    void BeginFrame();
    void DrawBox(const Box& box, COLORREF color);
    void DrawLine(int x1, int y1, int x2, int y2, COLORREF color, int width = 1);
    void DrawCircle(int cx, int cy, int radius, COLORREF color, int width = 2);
    void DrawStickFigure(int cx, int headY, int footY, COLORREF color);
    void DrawText(int x, int y, const wchar_t* text, COLORREF color);
    void DrawFilledRect(int l, int t, int r, int b, COLORREF fill, COLORREF border, int border_width);
    void EndFrame();

    HWND Handle() const { return m_hwnd; }
    bool Resize(int x, int y, int w, int h);
    void SetClickThrough(bool click_through);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void Paint();
    bool EnsureBuffer(int w, int h);

    HWND m_hwnd = nullptr;
    HDC m_memDC = nullptr;
    HBITMAP m_bmp = nullptr;
    HGDIOBJ m_oldBmp = nullptr;
    int m_w = 0;
    int m_h = 0;
    int m_x = 0;
    int m_y = 0;
    bool m_placed = false;
    ULONGLONG m_lastTopmost = 0;
    struct LineSeg { int x1, y1, x2, y2; COLORREF color; int width; };
    struct CircleItem { int cx, cy, radius; COLORREF color; int width; };
    struct TextItem { int x, y; const wchar_t* text; COLORREF color; };
    struct FillItem { int l, t, r, b; COLORREF fill; COLORREF border; int border_width; };

    void QueueLine(int x1, int y1, int x2, int y2, COLORREF color, int width);

    std::vector<std::pair<Box, COLORREF>> m_pending;
    std::vector<FillItem> m_fillPending;
    std::vector<LineSeg> m_linePending;
    std::vector<CircleItem> m_circlePending;
    std::vector<TextItem> m_textPending;
};
