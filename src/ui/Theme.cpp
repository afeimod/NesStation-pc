#include "ui/Theme.h"

#include "ui/Draw.h"

namespace ns {

UiStyle Theme::style_ = UiStyle::Neon;

const Skin& Theme::neon() {
  static const Skin s = [] {
    Skin k;
    k.bg = Color8::hex(0x05070E);
    k.bgDeep = Color8::hex(0x02030A);
    k.bgPanel = Color8::hex(0x0A1020, 0xD8);
    k.bgPanelHi = Color8::hex(0x101A30, 0xE8);
    k.line = Color8::hex(0x1C3350);
    k.accent = Color8::hex(0x00E5FF);
    k.accentDim = Color8::hex(0x00798C);
    k.red = Color8::hex(0xFF2D3F);
    k.blue = Color8::hex(0x2F7BD9);
    k.gold = Color8::hex(0xF2C200);
    k.green = Color8::hex(0x6DBE28);
    k.purple = Color8::hex(0x8B5CF6);
    k.textHi = Color8::hex(0xEAF6FF);
    k.text = Color8::hex(0xB8C7D9);
    k.textDim = Color8::hex(0x5E6F85);
    k.btnA = Color8::hex(0x6DBE28);
    k.btnB = Color8::hex(0xE23B3B);
    k.btnX = Color8::hex(0x2F7BD9);
    k.btnY = Color8::hex(0xF2C200);
    k.panelRadius = 8;
    k.chamfer = true;
    k.name = "Neon 3D 游戏站";
    return k;
  }();
  return s;
}

const Skin& Theme::fsd() {
  static const Skin s = [] {
    Skin k;
    k.bg = Color8::hex(0x0D2C55);
    k.bgDeep = Color8::hex(0x081426);
    k.bgPanel = Color8::hex(0x0D2C55, 0xD0);
    k.bgPanelHi = Color8::hex(0x123B6E, 0xE0);
    k.line = Color8::hex(0x2E6DB4);
    k.accent = Color8::hex(0x2E86E0);   // FSD 蓝
    k.accentDim = Color8::hex(0x1A5A9E);
    k.red = Color8::hex(0xE53935);
    k.blue = Color8::hex(0x2E86E0);
    k.gold = Color8::hex(0xF8E71C);     // FSD 黄
    k.green = Color8::hex(0x7ED321);
    k.purple = Color8::hex(0x8B5CF6);
    k.textHi = Color8::hex(0xE8F1FF);
    k.text = Color8::hex(0x9FB6D4);
    k.textDim = Color8::hex(0x4A5568);
    k.btnA = Color8::hex(0x7ED321);
    k.btnB = Color8::hex(0xE23B3B);
    k.btnX = Color8::hex(0x2E86E0);
    k.btnY = Color8::hex(0xF2C200);
    k.panelRadius = 10;
    k.chamfer = false;
    k.name = "FSD 经典 (Xbox360 磁贴)";
    return k;
  }();
  return s;
}

void Theme::setStyle(UiStyle s) { style_ = s; }

void skinPanel(Draw& d, const Rect& rc, bool highlighted, float glowSpread) {
  const Skin& sk = Theme::skin();
  if (glowSpread > 0 && highlighted) {
    d.glowRect(rc, sk.panelRadius, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 110),
               glowSpread);
  }
  if (sk.chamfer) {
    d.chamferRect(rc, rc.minDim() * 0.12f, highlighted ? sk.bgPanelHi : sk.bgPanel);
    d.chamferRectOutline(rc, rc.minDim() * 0.12f,
                         highlighted ? sk.accent : sk.line, highlighted ? 2.0f : 1.0f);
  } else {
    d.roundRect(rc, sk.panelRadius, highlighted ? sk.bgPanelHi : sk.bgPanel);
    d.roundRectOutline(rc, sk.panelRadius,
                       highlighted ? sk.accent : sk.line, highlighted ? 2.0f : 1.0f);
  }
}

}  // namespace ns
