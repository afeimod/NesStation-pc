#!/usr/bin/env python3
"""为无头自检准备数据：library.json + 合成封面 + assets 软链。"""
import json
import os
import shutil
from PIL import Image, ImageDraw

HOME = "/tmp/nesstation-test-home"
DATA = f"{HOME}/NesStation"
COVERS = f"{DATA}/covers"
BIN = "/tmp/nesbuild"

shutil.rmtree(HOME, ignore_errors=True)
os.makedirs(COVERS, exist_ok=True)
os.makedirs(f"{BIN}/assets", exist_ok=True)

# 字体软链（运行目录 assets/fonts）
if not os.path.exists(f"{BIN}/assets/fonts"):
    os.symlink("/home/z/my-project/NesStation-PC/assets/fonts", f"{BIN}/assets/fonts")
# 窗口图标
shutil.copy("/home/z/my-project/NesStation-PC/assets/icon32.png", f"{BIN}/assets/icon32.png")

games = [
    # (title, platform, color, lastPlayedHoursAgo, playMin, favorite)
    ("超级马力欧兄弟", "NES", (227, 76, 60), 2, 320, True),
    ("塞尔达传说 众神的三角力量", "SFC", (125, 60, 152), 26, 1180, False),
    ("宝可梦 红", "GB/GBC", (127, 176, 105), 50, 640, False),
    ("黄金的太阳", "GBA", (93, 109, 126), 200, 480, True),
    ("刺猬索尼克", "MD/SEGA", (62, 100, 175), 500, 90, False),
    ("最终幻想 VII", "PSX", (149, 165, 166), 900, 2600, False),
    ("勇者斗恶龙 Monsters", "NDS", (175, 122, 197), 72, 700, False),
    ("铁拳 3", "PSX", (149, 165, 166), 800, 300, False),
    # 3DS 游戏：进程内 azahar/citra 核心 + 外置 Azahar 独立核心并存
    # → 触发「双形态平台」核心选择器（pickerext 步骤）
    ("塞尔达传说 时之笛 3D", "3DS", (196, 90, 122), 10, 150, False),
]

# 测试 ROM（GameLibrary.load 要求 romPath 真实存在才会入库）
os.makedirs("/tmp/roms", exist_ok=True)

entries = []
now_h = 3600
for i, (title, plat, color, hrs, mins, fav) in enumerate(games):
    gid = f"testgame{i:02d}"
    rom = f"/tmp/roms/{gid}.zip"
    open(rom, "wb").write(b"PK\x05\x06" + b"\x00" * 12)  # 最小 zip（仅存在性检测用）
    cover = f"{COVERS}/{gid}.png"
    im = Image.new("RGB", (256, 348), color)
    dr = ImageDraw.Draw(im)
    dark = tuple(max(0, c - 70) for c in color)
    dr.rectangle([0, 268, 256, 348], fill=dark)
    # 简单像素装饰
    for y in range(20, 240, 28):
        dr.rectangle([16, y, 16 + (i * 13 + y) % 200, y + 12], fill=dark)
    im.save(cover)
    entries.append({
        "id": gid,
        "title": title,
        "customTitle": "",
        "romPath": rom,
        "coverPath": cover,
        "platform": plat,
        "lastPlayedAt": (0 if hrs > 700 else 3600 * (24 * 30 - hrs)),
        "playTimeSec": mins * 60,
        "favorite": fav,
    })

lib = {"version": 1, "games": entries, "romDirs": ["/tmp/roms"]}
with open(f"{DATA}/library.json", "w", encoding="utf-8") as f:
    json.dump(lib, f, ensure_ascii=False, indent=2)

# 假核心文件（多核心平台 → 触发核心选择器截图；内容为空，仅探测存在性）
# findCoreFile 查 <dataDir>/cores/<coreFile>.so（无 libretro 子目录）
_core_dir = f"{DATA}/cores"
os.makedirs(_core_dir, exist_ok=True)
for _c in ("fceumm_libretro.so", "nestopia_libretro.so", "quicknes_libretro.so",
           "snes9x_libretro.so", "bsnes_libretro.so",
           "mgba_libretro.so", "vba_next_libretro.so",
           "azahar_libretro.so", "citra_libretro.so"):
    open(f"{_core_dir}/{_c}", "w").close()

# 假外置核心安装（azahar → 触发「独立模拟器」页「已就绪 / 移除」状态截图；
# ExternalEmulators.detect 查 <dataDir>/external/<id>/<exeNames>）
_ext_dir = f"{DATA}/external/azahar"
os.makedirs(_ext_dir, exist_ok=True)
open(f"{_ext_dir}/azahar-qt", "w").close()
os.chmod(f"{_ext_dir}/azahar-qt", 0o755)

print("fixture ready:", len(entries), "games")
