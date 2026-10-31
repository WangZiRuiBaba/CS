#include "overlay.h"

#include <algorithm>

static constexpr wchar_t kClass[] = L"Cs2ScreenLabelGdi";

LRESULT CALLBACK GdiOverlay::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (auto* self = reinterpret_cast<GdiOverlay*>(GetWindowLongPtrW(h, GWLP_USERDATA))) {
            if (self->m_memDC)
                BitBlt(dc, 0, 0, self->m_w, self->m_h, self->m_memDC, 0, 0, SRCCOPY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_CLOSE || msg == WM_DESTROY)
        return 0;
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_CLOSE)
        return 0;
    return DefWindowProcW(h, msg, wp, lp);
}

bool GdiOverlay::EnsureBuffer(int w, int h) {
    if (w <= 0 || h <= 0)
        return false;
    if (m_memDC && m_w == w && m_h == h)
        return true;

    if (m_memDC) {
        if (m_oldBmp)
            SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_bmp);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
        m_bmp = nullptr;
        m_oldBmp = nullptr;
    }

    m_w = w;
    m_h = h;
    HDC screen = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screen);
    m_bmp = CreateCompatibleBitmap(screen, m_w, m_h);
    m_oldBmp = SelectObject(m_memDC, m_bmp);
    ReleaseDC(nullptr, screen);
    return m_memDC != nullptr;
}

bool GdiOverlay::Create() {
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = &GdiOverlay::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    UnregisterClassW(kClass, wc.hInstance);
    if (!RegisterClassExW(&wc))
        return false;

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClass, L"", WS_POPUP, 0, 0, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);
    if (!m_hwnd)
        return false;

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    if (!EnsureBuffer(800, 600))
        return false;

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

bool GdiOverlay::Resize(int x, int y, int w, int h) {
    if (!m_hwnd)
        return false;
    if (m_placed && m_x == x && m_y == y && m_w == w && m_h == h)
        return true;
    if (!EnsureBuffer(w, h))
        return false;
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_x = x;
    m_y = y;
    m_placed = true;
    m_lastTopmost = GetTickCount64();
    return true;
}

void GdiOverlay::Destroy() {
    if (m_memDC) {
        if (m_oldBmp)
            SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_bmp);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_hwnd) {
        HWND dead = m_hwnd;
        m_hwnd = nullptr;
        if (IsWindow(dead))
            DestroyWindow(dead);
    }
    UnregisterClassW(kClass, GetModuleHandleW(nullptr));
}

void GdiOverlay::BeginFrame() {
    m_pending.clear();
    m_fillPending.clear();
    m_linePending.clear();
    m_circlePending.clear();
    m_textPending.clear();
}

void GdiOverlay::QueueLine(int x1, int y1, int x2, int y2, COLORREF color, int width) {
    m_linePending.push_back({ x1, y1, x2, y2, color, std::max(1, width) });
}

void GdiOverlay::DrawBox(const Box& box, COLORREF color) { m_pending.emplace_back(box, color); }

void GdiOverlay::DrawLine(int x1, int y1, int x2, int y2, COLORREF color, int width) {
    QueueLine(x1, y1, x2, y2, color, width);
}

void GdiOverlay::DrawCircle(int cx, int cy, int radius, COLORREF color, int width) {
    if (radius <= 0)
        return;
    m_circlePending.push_back({ cx, cy, radius, color, std::max(1, width) });
}

void GdiOverlay::DrawStickFigure(int cx, int headY, int footY, COLORREF color) {
    if (footY <= headY)
        return;

    const int h = footY - headY;
    if (h < 10)
        return;

    const int pen = (h >= 120) ? 3 : (h >= 48 ? 2 : 1);
    const int headR = std::max(3, h * 9 / 100);
    const int headCy = headY + headR;

    m_circlePending.push_back({ cx, headCy, headR, color, pen });

    const int neckY = headCy + headR;
    const int shoulderY = footY - h * 28 / 100;
    const int hipY = footY - h * 52 / 100;
    const int shoulderHalf = std::max(4, h * 20 / 100);
    const int armDrop = std::max(6, h * 30 / 100);
    const int armOut = std::max(4, h * 12 / 100);
    const int legSpread = std::max(4, h * 14 / 100);

    QueueLine(cx, neckY, cx, hipY, color, pen);
    QueueLine(cx - shoulderHalf, shoulderY, cx + shoulderHalf, shoulderY, color, pen);
    QueueLine(cx - shoulderHalf, shoulderY, cx - shoulderHalf - armOut, shoulderY + armDrop, color, pen);
    QueueLine(cx + shoulderHalf, shoulderY, cx + shoulderHalf + armOut, shoulderY + armDrop, color, pen);
    QueueLine(cx, hipY, cx - legSpread, footY, color, pen);
    QueueLine(cx, hipY, cx + legSpread, footY, color, pen);
}

void GdiOverlay::DrawText(int x, int y, const wchar_t* text, COLORREF color) {
    if (text && text[0])
        m_textPending.push_back({ x, y, text, color });
}

void GdiOverlay::DrawFilledRect(int l, int t, int r, int b, COLORREF fill, COLORREF border,
                                int border_width) {
    if (r <= l || b <= t)
        return;
    m_fillPending.push_back({ l, t, r, b, fill, border, std::max(0, border_width) });
}

void GdiOverlay::SetClickThrough(bool click_through) {
    if (!m_hwnd)
        return;
    LONG ex = GetWindowLongW(m_hwnd, GWL_EXSTYLE);
    if (click_through)
        ex |= WS_EX_TRANSPARENT;
    else
        ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongW(m_hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void GdiOverlay::EndFrame() {
    Paint();
    if (m_hwnd) {
        const ULONGLONG now = GetTickCount64();
        if (now - m_lastTopmost >= 500) {
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            m_lastTopmost = now;
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        UpdateWindow(m_hwnd);
    }
}

void GdiOverlay::Paint() {
    if (!m_memDC || m_w <= 0 || m_h <= 0)
        return;

    RECT full{ 0, 0, m_w, m_h };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_memDC, &full, bg);
    DeleteObject(bg);

    for (auto& [box, color] : m_pending) {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        HGDIOBJ oldPen = SelectObject(m_memDC, pen);
        HGDIOBJ oldBrush = SelectObject(m_memDC, GetStockObject(HOLLOW_BRUSH));
        Rectangle(m_memDC, box.l, box.t, box.r, box.b);
        SelectObject(m_memDC, oldBrush);
        SelectObject(m_memDC, oldPen);
        DeleteObject(pen);
    }

    for (const auto& f : m_fillPending) {
        HBRUSH brush = CreateSolidBrush(f.fill);
        HGDIOBJ oldBrush = SelectObject(m_memDC, brush);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, f.border_width), f.border);
        HGDIOBJ oldPen = SelectObject(m_memDC, pen);
        Rectangle(m_memDC, f.l, f.t, f.r, f.b);
        SelectObject(m_memDC, oldPen);
        SelectObject(m_memDC, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    for (const auto& seg : m_linePending) {
        HPEN pen = CreatePen(PS_SOLID, seg.width, seg.color);
        HGDIOBJ oldPen = SelectObject(m_memDC, pen);
        MoveToEx(m_memDC, seg.x1, seg.y1, nullptr);
        LineTo(m_memDC, seg.x2, seg.y2);
        SelectObject(m_memDC, oldPen);
        DeleteObject(pen);
    }

    for (const auto& c : m_circlePending) {
        HPEN pen = CreatePen(PS_SOLID, c.width, c.color);
        HGDIOBJ oldPen = SelectObject(m_memDC, pen);
        HGDIOBJ oldBrush = SelectObject(m_memDC, GetStockObject(HOLLOW_BRUSH));
        Ellipse(m_memDC, c.cx - c.radius, c.cy - c.radius, c.cx + c.radius, c.cy + c.radius);
        SelectObject(m_memDC, oldBrush);
        SelectObject(m_memDC, oldPen);
        DeleteObject(pen);
    }

    if (!m_textPending.empty()) {
        HFONT font = CreateFontW(
            16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(m_memDC, font);
        SetBkMode(m_memDC, TRANSPARENT);
        for (auto& [x, y, text, color] : m_textPending) {
            SetTextColor(m_memDC, color);
            TextOutW(m_memDC, x, y, text, static_cast<int>(lstrlenW(text)));
        }
        SelectObject(m_memDC, oldFont);
        DeleteObject(font);
    }
}
