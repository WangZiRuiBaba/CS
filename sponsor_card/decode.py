# -*- coding: utf-8 -*-
"""解出仓库收款码的载荷，并定位二维码区域。"""
import zxingcpp
from PIL import Image

im = Image.open(r"E:\WorkflowGame\CS\docs\donate\wechat-pay.jpg").convert("RGB")
print("size", im.size)
res = zxingcpp.read_barcodes(im)
for r in res:
    print("format:", r.format)
    print("text  :", repr(r.text))
    try:
        print("bytes :", r.bytes)
    except Exception as e:
        print("bytes : <n/a>", e)
    p = r.position
    print("pos   :", p.top_left, p.top_right, p.bottom_right, p.bottom_left)
print("count", len(res))
