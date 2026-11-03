# -*- coding: utf-8 -*-
"""生成底图与素材：poster_base.png（白卡留白、无码无徽标）、assets/badge.png、assets/avatar.png"""
import os
import numpy as np
from PIL import Image, ImageDraw

SRC = r"C:\Users\Administrator\.dsh\attachments\v1\objects\f6\f6369d574177800d93187693246a4ed04d3675a33fc51218ca53fbfc5735d580"
OUT = os.path.dirname(os.path.abspath(__file__))
A = os.path.join(OUT, "assets")
os.makedirs(A, exist_ok=True)

im = Image.open(SRC).convert("RGB")
CARD = (318, 183, 834, 699)      # 白卡外沿（左,上,右,下 开区间）

# 1) 干净底图：白卡整体刷白
base = im.copy()
ImageDraw.Draw(base).rectangle((CARD[0], CARD[1], CARD[2] - 1, CARD[3] - 1), fill=(255, 255, 255))
base.save(os.path.join(OUT, "poster_base.png"))

# 2) 徽标：圆形遮罩裁下 👍（圆心 725,590，r=46）
S, r = 100, 46
cx, cy = 725, 590
box = (cx - S // 2, cy - S // 2, cx + S // 2, cy + S // 2)
badge = im.crop(box).convert("RGBA")
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).ellipse((S // 2 - r, S // 2 - r, S // 2 + r - 1, S // 2 + r - 1), fill=255)
badge.putalpha(mask)
badge.save(os.path.join(A, "badge.png"))

# 3) 头像：184x184 @ (484,349)，圆角 16
av = im.crop((484, 349, 668, 533)).convert("RGBA")
m2 = Image.new("L", av.size, 0)
ImageDraw.Draw(m2).rounded_rectangle((0, 0, av.size[0] - 1, av.size[1] - 1), radius=16, fill=255)
av.putalpha(m2)
av.save(os.path.join(A, "avatar.png"))
print("poster_base.png / assets/badge.png / assets/avatar.png 就绪")
