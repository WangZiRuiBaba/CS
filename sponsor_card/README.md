# 赞赏码换码器（poster → 任意新码）

原海报（深色 + 白卡 + 黄带）已拆成「底图 + 码层」，中心那张码可以随时换成任何
二维码 / 赞赏码 / 赞助码截图，版式、👍 角标、引号文案、底部黄带全部保留。

## 文件

| 文件 | 用途 |
| --- | --- |
| `poster_base.png` | 干净底版：白卡留白、👍 已抠掉，直接贴码就能出片 |
| `assets/avatar.png` | 原图中心头像（184×184，圆角），做码心 logo |
| `assets/badge.png` | 原图右下 👍 金色徽标（圆形抠图，透明底） |
| `build_card.py` | 主脚本：码 + 底版 → 成品 |
| `make_base.py` | 从原图重切底版与素材（素材丢了才需要跑） |
| `verify.py` | 用 zxing-cpp 解码成品，确认扫得出来 |
| `out/demo_card.png` | 示例成品（码内容为占位链接，**已验可扫**） |
| `out/demo_card_sponsor.png` | 同上，底栏文案改成「我的歌 的赞助码」 |
| `out/template_blank.png` | 空白模板（无码），想手动贴码用这张 |

## 用法

```bash
PY="C:/Users/Administrator/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/python.exe"

# 1) 换成你自己的码（截图/导出的二维码都行，自动裁白边、等比适配、居中）
"$PY" build_card.py --qr D:/my_code.png --out out/my_card.png

# 2) 顺带改底栏文案
"$PY" build_card.py --qr D:/my_code.png --bottom-text "我的歌 的赞助码" --out out/my_card.png

# 3) 出个可扫的示例码（占位链接），看看效果
"$PY" build_card.py --qr demo --payload "https://example.com/your-sponsor-link" --out out/demo.png

# 4) 出可扫性校验（整图 + 裁切，两种方式）
"$PY" verify.py out/my_card.png
```

## 参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--qr` | `demo` | 码图路径；`demo` 现生成；`none` 不放码 |
| `--payload` | 占位链接 | `demo` 码的内容 |
| `--logo` | `assets/avatar.png` | 码心头像；`none` 不放 |
| `--style` | `dots` | `dots` 圆点风（跟原图调性一致）/ `square` 方点 |
| `--size` | `440` | 码边长，白卡 516，建议 380–460 |
| `--offset` | `0,0` | 码中心微调 `dx,dy` |
| `--badge` | `auto` | `auto` 新码自带金标就不叠；`on` 强制叠；`off` 不叠 |
| `--bottom-text` | 不改 | 改写底栏白字 |
| `--base` | `poster_base.png` | 换底版（做别的配色/尺寸时用） |

## 踩过的坑（别再踩）

- **码心 logo 别超过矩阵边长 24%**：37×37 矩阵、EC=H 时，0.34 会让 zxing 直接解不出，
  0.30 勉强、0.24 稳。`--logo none` 最保险。
- **圆点风可扫**：模块渲成直径 ≈0.88×模块的圆点，静区留 4 模块，432–410 px 时 360 px
  缩图仍能解。
- **别重复叠徽标**：微信赞赏码原图自带 👍，直接贴会变双金标，所以默认 `--badge auto`。
- 输入码图分辨率别低于 300×300，否则放大后模块糊成一团。
