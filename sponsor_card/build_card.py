# -*- coding: utf-8 -*-
"""
赞赏码/赞助码 换码器
--------------------
把 poster_base.png（原版海报，中间白卡已留白）与新码合成，输出成品图。

用法示例
    # 1) 用你自己的码图（截图/导出的二维码图片都行，自动裁白边、等比适配）
    python build_card.py --qr D:\\my_code.png --out out/my_card.png

    # 2) 生成一个可扫的示例赞助码并合成（占位链接）
    python build_card.py --qr demo --payload "https://example.com/your-sponsor-link" --out out/demo_card.png

    # 3) 只留干净模板，不放码
    python build_card.py --qr none --out out/template.png
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
CARD = (318, 183, 834, 699)          # 白卡
CARD_CENTER = ((CARD[0] + CARD[2]) // 2, (CARD[1] + CARD[3]) // 2)
BAND_RGB = (224, 183, 65)            # 底部黄带
BG_RGB = (25, 25, 25)                # 背景
INK = (17, 17, 17)                   # 码点颜色
FONT_BOLD = r"C:\Windows\Fonts\msyh.ttc"
FONT_LIGHT = r"C:\Windows\Fonts\msyhl.ttc"


# ---------------------------------------------------------------- 工具函数
def trim_border(img, tol=12):
    """裁掉图片四周纯色（白/透明）边框。"""
    img = img.convert("RGBA")
    if img.getextrema()[3][0] < 255:                      # 有透明区 -> 用 alpha 定位
        bbox = img.split()[3].point(lambda v: 255 if v > 8 else 0).getbbox()
        return img.crop(bbox) if bbox else img
    gray = img.convert("L")
    w, h = gray.size
    px = gray.load()
    bg = px[0, 0]

    def row_blank(y):
        return all(abs(px[x, y] - bg) <= tol for x in range(0, w, max(1, w // 200)))

    def col_blank(x):
        return all(abs(px[x, y] - bg) <= tol for y in range(0, h, max(1, h // 200)))

    top, bottom, left, right = 0, h - 1, 0, w - 1
    while top < bottom and row_blank(top):
        top += 1
    while bottom > top and row_blank(bottom):
        bottom -= 1
    while left < right and col_blank(left):
        left += 1
    while right > left and col_blank(right):
        right -= 1
    return img.crop((left, top, right + 1, bottom + 1))


def make_demo_matrix(payload):
    import segno
    qr = segno.make(payload, error="h")
    return [list(row) for row in qr.matrix]


def render_code(matrix, size, style="dots"):
    """把 0/1 矩阵渲染成 size×size 的码图（带 4 模块静区），风格 dots 用圆点。"""
    m = len(matrix)
    quiet = 4
    total = m + 2 * quiet
    scale = max(1, size // total)
    side = scale * total
    img = Image.new("RGBA", (side, side), (255, 255, 255, 255))
    d = ImageDraw.Draw(img)

    def is_finder(r, c):
        return (r < 7 and c < 7) or (r < 7 and c >= m - 7) or (r >= m - 7 and c < 7)

    for r in range(m):
        for c in range(m):
            if not matrix[r][c]:
                continue
            x = (quiet + c) * scale
            y = (quiet + r) * scale
            if style == "square" or is_finder(r, c):
                d.rectangle((x, y, x + scale - 1, y + scale - 1), fill=INK)
            else:
                pad = max(0, int(scale * 0.06))
                d.ellipse((x + pad, y + pad, x + scale - 1 - pad, y + scale - 1 - pad), fill=INK)
    return img


def compose_code(qr_path, size, logo_path, style, payload, logo_ratio=0.24):
    """返回 size×size 的成品码（含中心 logo）。logo_ratio 相对矩阵边长，>0.30 会解不出来。"""
    if qr_path == "demo":
        src = render_code(make_demo_matrix(payload), size, style)
    else:
        raw = trim_border(Image.open(qr_path))
        # 等比缩放进 size×size
        k = min(size / raw.width, size / raw.height)
        w, h = max(1, int(raw.width * k)), max(1, int(raw.height * k))
        src = Image.new("RGBA", (size, size), (255, 255, 255, 255))
        src.paste(raw.resize((w, h), Image.LANCZOS), ((size - w) // 2, (size - h) // 2), raw.resize((w, h), Image.LANCZOS))

    if logo_path and os.path.exists(logo_path):
        logo = Image.open(logo_path).convert("RGBA")
        side = int(size * logo_ratio)
        logo = logo.resize((side, side), Image.LANCZOS)
        pad = max(5, int(side * 0.06))
        plate = Image.new("RGBA", (side + 2 * pad, side + 2 * pad), (0, 0, 0, 0))
        ImageDraw.Draw(plate).rounded_rectangle(
            (0, 0, plate.width - 1, plate.height - 1), radius=int(side * 0.18), fill=(255, 255, 255, 255))
        plate.paste(logo, (pad, pad), logo)
        src = src.convert("RGBA")
        src.paste(plate, ((size - plate.width) // 2, (size - plate.height) // 2), plate)
    return src


def draw_text_center(img, text, center, font_path, size, fill):
    f = ImageFont.truetype(font_path, size)
    d = ImageDraw.Draw(img)
    box = d.textbbox((0, 0), text, font=f)
    x = center[0] - (box[2] - box[0]) / 2 - box[0]
    y = center[1] - (box[3] - box[1]) / 2 - box[1]
    d.text((x, y), text, font=f, fill=fill)


# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qr", default="demo", help="码图路径 / demo / none")
    ap.add_argument("--payload", default="https://example.com/your-sponsor-link", help="demo 码内容")
    ap.add_argument("--logo", default=os.path.join(HERE, "assets", "avatar.png"), help="中心头像；none 表示不放")
    ap.add_argument("--style", default="dots", choices=["dots", "square"])
    ap.add_argument("--size", type=int, default=440, help="码边长（白卡 516）")
    ap.add_argument("--offset", default="0,0", help="码中心偏移 dx,dy")
    ap.add_argument("--base", default=os.path.join(HERE, "poster_base.png"))
    ap.add_argument("--badge", default="auto", choices=["auto", "on", "off"], help="右下角 👍 徽标")
    ap.add_argument("--bottom-text", default=None, help="改写底部文字，如 '我的歌 的赞助码'")
    ap.add_argument("--out", default=os.path.join(HERE, "out", "card.png"))
    a = ap.parse_args()

    poster = Image.open(a.base).convert("RGBA")

    if a.qr != "none":
        logo = None if a.logo.lower() == "none" else a.logo
        code = compose_code(a.qr, a.size, logo, a.style, a.payload)
        dx, dy = [int(v) for v in a.offset.split(",")]
        pos = (CARD_CENTER[0] - a.size // 2 + dx, CARD_CENTER[1] - a.size // 2 + dy)
        poster.paste(code, pos, code)

    if a.badge != "off":
        badge_box = (675, 540, 775, 640)
        # auto：新码自带金色徽标（如微信赞赏码原图）就不再叠一个
        has_gold = False
        if a.badge == "auto":
            m = poster.crop(badge_box).convert("RGB")
            has_gold = sum(1 for p in m.getdata() if p[0] > 150 and p[2] < 150 and p[0] - p[2] > 50) > 300
        if not has_gold:
            badge = Image.open(os.path.join(HERE, "assets", "badge.png")).convert("RGBA")
            poster.paste(badge, (675, 540), badge)

    if a.bottom_text:
        d = ImageDraw.Draw(poster)
        d.rectangle((360, 1030, 800, 1095), fill=BAND_RGB)
        draw_text_center(poster, a.bottom_text, (576, 1062), FONT_LIGHT, 50, (255, 255, 255))

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    poster.convert("RGB").save(a.out, quality=96)
    print("saved", a.out, poster.size)


if __name__ == "__main__":
    main()
