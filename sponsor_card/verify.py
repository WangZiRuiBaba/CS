# -*- coding: utf-8 -*-
"""成品可扫性校验：整图 / 裁码区 两种方式解码。"""
import sys
import zxingcpp
from PIL import Image

path = sys.argv[1] if len(sys.argv) > 1 else "out/demo_card.png"
im = Image.open(path).convert("RGB")
res = zxingcpp.read_barcodes(im)
print("[整图]", [(r.format, r.text) for r in res] or "解码失败")
crop = im.crop((360, 225, 800, 660))
res2 = zxingcpp.read_barcodes(crop)
print("[裁切]", [(r.format, r.text) for r in res2] or "解码失败")
