#include "ui/Font.h"

#include <algorithm>
#include <cstring>

#include <stb_truetype.h>

#include "util/Log.h"

namespace ns {

std::vector<uint32_t> utf8ToCodepoints(const std::string& s) {
  std::vector<uint32_t> out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    const uint8_t c = (uint8_t)s[i];
    uint32_t cp = c;
    size_t len = 1;
    if ((c & 0x80) == 0) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = ((uint32_t)(c & 0x1F) << 6) | (s[i + 1] & 0x3F);
      len = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[i + 1] & 0x3F) << 6) |
           (s[i + 2] & 0x3F);
      len = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
      cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[i + 1] & 0x3F) << 12) |
           ((uint32_t)(s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
      len = 4;
    }
    if (i + len > s.size()) break;
    out.push_back(cp);
    i += len;
  }
  return out;
}

size_t utf8Len(const std::string& s) { return utf8ToCodepoints(s).size(); }

namespace {
std::string codepointsToUtf8(const std::vector<uint32_t>& cps) {
  std::string s;
  for (uint32_t cp : cps) {
    if (cp < 0x80) {
      s.push_back((char)cp);
    } else if (cp < 0x800) {
      s.push_back((char)(0xC0 | (cp >> 6)));
      s.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      s.push_back((char)(0xE0 | (cp >> 12)));
      s.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
      s.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
      s.push_back((char)(0xF0 | (cp >> 18)));
      s.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
      s.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
      s.push_back((char)(0x80 | (cp & 0x3F)));
    }
  }
  return s;
}
}  // namespace

FontCache::~FontCache() {
  for (auto& kv : atlases_)
    if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
  if (regFont_) delete reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (boldFont_) delete reinterpret_cast<stbtt_fontinfo*>(boldFont_);
}

bool FontCache::load(const std::string& regularTtf, const std::string& boldTtf) {
  auto loadFile = [](const std::string& p, std::vector<uint8_t>* out) -> bool {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
      fclose(f);
      return false;
    }
    out->resize((size_t)sz);
    const size_t rd = fread(out->data(), 1, out->size(), f);
    fclose(f);
    return rd == out->size();
  };

  regData_.clear();
  boldData_.clear();
  if (!loadFile(regularTtf, &regData_)) {
    LOGE("字体加载失败：%s", regularTtf.c_str());
    return false;
  }
  if (regFont_) delete reinterpret_cast<stbtt_fontinfo*>(regFont_);
  auto* reg = new stbtt_fontinfo();
  if (stbtt_InitFont(reg, regData_.data(), 0) == 0) {
    LOGE("stbtt_InitFont 失败（regular）");
    delete reg;
    return false;
  }
  regFont_ = reg;

  if (!boldTtf.empty() && loadFile(boldTtf, &boldData_)) {
    if (boldFont_) delete reinterpret_cast<stbtt_fontinfo*>(boldFont_);
    auto* bold = new stbtt_fontinfo();
    if (stbtt_InitFont(bold, boldData_.data(), 0) != 0) {
      boldFont_ = bold;
    } else {
      delete bold;
      boldFont_ = nullptr;
      LOGW("粗体初始化失败，回退常规体");
    }
  } else {
    LOGW("粗体缺失，回退常规体：%s", boldTtf.c_str());
  }
  LOGI("字体就绪（regular %zu 字节）", regData_.size());
  return true;
}

FontCache::Atlas& FontCache::atlasFor(float size, bool bold) {
  const uint64_t key = ((uint64_t)(int)(size * 2.0f) << 1) | (bold ? 1u : 0u);
  return atlases_[key];
}

bool FontCache::atlasPlace(Atlas& atlas, int w, int h) {
  if (w + 2 >= atlas.size) return true;   // 异常大字形：跳过绘制
  if (h + 2 >= atlas.size) return true;
  if (atlas.cursorX + w + 1 >= atlas.size) {
    atlas.cursorY += atlas.rowH + 1;
    atlas.cursorX = 1;
    atlas.rowH = 0;
  }
  return atlas.cursorY + h + 1 < atlas.size;
}

const FontCache::Glyph& FontCache::bake(Atlas& atlas, float size, bool bold, uint32_t cp) {
  auto it = atlas.glyphs.find(cp);
  if (it != atlas.glyphs.end()) {
    // 防御：历史上经无渲染器路径只测量未上传的字形（px=py=0 且有位图），
    // 现在渲染器就绪 → 立即补上传（真实放置坐标恒 ≥1，0,0 不会误判）
    if (it->second.w > 0 && it->second.h > 0 && it->second.px == 0 &&
        it->second.py == 0 && bakeRenderer_) {
      if (!atlas.tex) {
        atlas.tex = SDL_CreateTexture(bakeRenderer_, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, atlas.size, atlas.size);
        if (atlas.tex) {
          std::vector<uint32_t> zeros((size_t)atlas.size * atlas.size, 0);
          SDL_UpdateTexture(atlas.tex, nullptr, zeros.data(), atlas.size * 4);
          SDL_SetTextureBlendMode(atlas.tex, SDL_BLENDMODE_BLEND);
        }
      }
      if (atlas.tex) {
        stbtt_fontinfo* f2 =
            reinterpret_cast<stbtt_fontinfo*>((bold && boldFont_) ? boldFont_ : regFont_);
        if (!f2) f2 = reinterpret_cast<stbtt_fontinfo*>(regFont_);
        int w2 = 0, h2 = 0;
        unsigned char* bmp2 = stbtt_GetCodepointBitmap(
            f2, 0, stbtt_ScaleForPixelHeight(f2, size), cp, &w2, &h2, nullptr, nullptr);
        if (bmp2 && w2 > 0 && h2 > 0 && atlasPlace(atlas, w2, h2)) {
          std::vector<uint32_t> buf((size_t)w2 * h2);
          for (int i = 0; i < w2 * h2; i++)
            buf[i] = ((uint32_t)bmp2[i] << 24) | 0x00FFFFFFu;
          SDL_Rect r2{atlas.cursorX, atlas.cursorY, w2, h2};
          SDL_UpdateTexture(atlas.tex, &r2, buf.data(), w2 * 4);
          it->second.px = atlas.cursorX;
          it->second.py = atlas.cursorY;
          it->second.u0 = (float)atlas.cursorX / atlas.size;
          it->second.v0 = (float)atlas.cursorY / atlas.size;
          it->second.u1 = (float)(atlas.cursorX + w2) / atlas.size;
          it->second.v1 = (float)(atlas.cursorY + h2) / atlas.size;
          atlas.cursorX += w2 + 1;
          atlas.rowH = std::max(atlas.rowH, h2);
        }
        if (bmp2) stbtt_FreeBitmap(bmp2, nullptr);
      }
    }
    return it->second;
  }

  stbtt_fontinfo* font =
      (bold && boldFont_) ? reinterpret_cast<stbtt_fontinfo*>(boldFont_)
                          : reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (!font) font = reinterpret_cast<stbtt_fontinfo*>(regFont_);

  const float scale = stbtt_ScaleForPixelHeight(font, size);
  int w = 0, h = 0, xoff = 0, yoff = 0;
  unsigned char* bmp = stbtt_GetCodepointBitmap(font, 0, scale, cp, &w, &h, &xoff, &yoff);
  int adv = 0, lsb = 0;
  stbtt_GetCodepointHMetrics(font, cp, &adv, &lsb);

  Glyph g;
  g.w = w;
  g.h = h;
  g.xoff = (float)xoff;
  g.yoff = (float)yoff;
  g.advance = (float)adv * scale;

  const bool needDraw = w > 0 && h > 0 && w < atlas.size && h < atlas.size;
  if (needDraw) {
    if (!atlasPlace(atlas, w, h)) {
      // 扩容：销毁重建，全部字形重烘焙
      std::vector<uint32_t> keep;
      keep.reserve(atlas.glyphs.size() + 1);
      for (auto& kv : atlas.glyphs) keep.push_back(kv.first);
      if (std::find(keep.begin(), keep.end(), cp) == keep.end()) keep.push_back(cp);
      if (atlas.tex) SDL_DestroyTexture(atlas.tex);
      atlas.tex = nullptr;
      atlas.size = atlas.size * 2 > 4096 ? 4096 : atlas.size * 2;
      atlas.cursorX = 1;
      atlas.cursorY = 1;
      atlas.rowH = 0;
      atlas.glyphs.clear();
      stbtt_FreeBitmap(bmp, nullptr);
      for (uint32_t k : keep) bake(atlas, size, bold, k);
      return atlas.glyphs.count(cp) ? atlas.glyphs[cp] : g;
    }

    if (!atlas.tex && bakeRenderer_) {
      atlas.tex = SDL_CreateTexture(bakeRenderer_, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, atlas.size, atlas.size);
      if (atlas.tex) {
        std::vector<uint32_t> zeros((size_t)atlas.size * atlas.size, 0);
        SDL_UpdateTexture(atlas.tex, nullptr, zeros.data(), atlas.size * 4);
        SDL_SetTextureBlendMode(atlas.tex, SDL_BLENDMODE_BLEND);
      }
    }
    if (atlas.tex && bmp) {
      std::vector<uint32_t> buf((size_t)w * h);
      for (int i = 0; i < w * h; i++) {
        const unsigned char a = bmp[i];
        buf[i] = ((uint32_t)a << 24) | 0x00FFFFFFu;
      }
      SDL_Rect r{atlas.cursorX, atlas.cursorY, w, h};
      SDL_UpdateTexture(atlas.tex, &r, buf.data(), w * 4);
      g.px = atlas.cursorX;
      g.py = atlas.cursorY;
      g.u0 = (float)atlas.cursorX / atlas.size;
      g.v0 = (float)atlas.cursorY / atlas.size;
      g.u1 = (float)(atlas.cursorX + w) / atlas.size;
      g.v1 = (float)(atlas.cursorY + h) / atlas.size;
    }
    atlas.cursorX += w + 1;
    atlas.rowH = std::max(atlas.rowH, h);
  }
  if (bmp) stbtt_FreeBitmap(bmp, nullptr);

  return atlas.glyphs.emplace(cp, g).first->second;
}

float FontCache::draw(SDL_Renderer* r, const std::string& utf8, float x, float y,
                      float size, Color8 color, bool bold) {
  Atlas& atlas = atlasFor(size, bold);
  bakeRenderer_ = r;
  const auto cps = utf8ToCodepoints(utf8);
  float penX = x;
  for (uint32_t cp : cps) {
    if (cp == '\n') continue;
    const Glyph& g = bake(atlas, size, bold, cp);
    if (g.w > 0 && g.h > 0 && atlas.tex) {
      SDL_SetTextureColorMod(atlas.tex, color.r, color.g, color.b);
      SDL_SetTextureAlphaMod(atlas.tex, color.a);
      const float baseline = y + size * 0.8f;
      const SDL_Rect src{g.px, g.py, g.w, g.h};
      const SDL_Rect dst{(int)(penX + g.xoff), (int)(baseline + g.yoff), g.w, g.h};
      SDL_RenderCopy(r, atlas.tex, &src, &dst);
    }
    penX += g.advance;
  }
  return penX - x;
}

float FontCache::drawVCentered(SDL_Renderer* r, const std::string& utf8, float x, float cy,
                               float size, Color8 color, bool bold) {
  return draw(r, utf8, x, cy - size * 0.5f, size, color, bold);
}

float FontCache::textWidth(const std::string& utf8, float size, bool bold) {
  // 直接用 stbtt 度量，不烘焙字形：
  // bake() 需要渲染器（bakeRenderer_）才能建图集纹理；若在首次 draw 之前
  // 经 textWidth 烘焙，字形会以 px=py=0 入缓存却从未上传 —— 永久不可见
  // （曾导致主页徽标 NES·STATION 整串消失）。
  stbtt_fontinfo* font =
      (bold && boldFont_) ? reinterpret_cast<stbtt_fontinfo*>(boldFont_)
                          : reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (!font) font = reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (!font) return 0;
  const float scale = stbtt_ScaleForPixelHeight(font, size);
  const auto cps = utf8ToCodepoints(utf8);
  float w = 0;
  for (uint32_t cp : cps) {
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(font, cp, &adv, &lsb);
    w += (float)adv * scale;
  }
  return w;
}

float FontCache::lineHeight(float size) const { return size * 1.35f; }

std::vector<std::string> FontCache::wrap(const std::string& utf8, float size,
                                         float maxWidth, bool bold) {
  std::vector<std::string> lines;
  const auto cps = utf8ToCodepoints(utf8);
  std::vector<uint32_t> cur;
  float w = 0;
  auto flush = [&]() {
    if (cur.empty()) return;
    lines.push_back(codepointsToUtf8(cur));
    cur.clear();
    w = 0;
  };
  Atlas& atlas = atlasFor(size, bold);
  for (uint32_t cp : cps) {
    if (cp == '\n') {
      flush();
      continue;
    }
    const Glyph& g = bake(atlas, size, bold, cp);
    if (w + g.advance > maxWidth && !cur.empty()) flush();
    cur.push_back(cp);
    w += g.advance;
  }
  flush();
  return lines;
}

std::string FontCache::ellipsize(const std::string& utf8, float size, float maxWidth,
                                 bool bold) {
  if (textWidth(utf8, size, bold) <= maxWidth) return utf8;
  const float dots = textWidth("…", size, bold);
  const auto cps = utf8ToCodepoints(utf8);
  Atlas& atlas = atlasFor(size, bold);
  float w = 0;
  std::vector<uint32_t> out;
  for (uint32_t cp : cps) {
    const Glyph& g = bake(atlas, size, bold, cp);
    if (w + g.advance + dots > maxWidth) break;
    out.push_back(cp);
    w += g.advance;
  }
  return codepointsToUtf8(out) + "…";
}

}  // namespace ns
