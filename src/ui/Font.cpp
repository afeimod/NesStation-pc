#include "ui/Font.h"

#include <algorithm>
#include <cstring>

#include <stb_truetype.h>

#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

// ★v1.2.7 设备像素烘焙：设计字号 × 全局 UI 缩放 = 实际烘焙/度量像素。
// 渲染坐标系为设计单位（渲染器逻辑尺寸 = 输出像素 / 缩放），字形位图
// 按设备像素烘焙后 1:1 落盘，不再被 GPU 二次放大 → 高 DPI 下不再发糊。
static float fontDeviceSize(float designSize) {
  return designSize * uiscale::get();
}

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
    FILE* f = fsutil::openFile(p, "rb");
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
  // ★TTC 集合支持：系统字体常见 .ttc（msyh.ttc / NotoSansCJK-*.ttc / simsun.ttc），
  //   stbtt_InitFont 的 offset 参数必须传 stbtt_GetFontOffsetForIndex(data, 0)
  //   的返回值（普通 .ttf 时为 0），直接传 0 解析 .ttc 必失败。
  auto initFont = [](void** slot, const std::vector<uint8_t>& data) -> bool {
    if (*slot) {
      delete reinterpret_cast<stbtt_fontinfo*>(*slot);
      *slot = nullptr;
    }
    if (data.empty()) return false;
    const int offset = stbtt_GetFontOffsetForIndex(data.data(), 0);
    if (offset < 0) return false;
    auto* info = new stbtt_fontinfo();
    if (stbtt_InitFont(info, data.data(), offset) == 0) {
      delete info;
      return false;
    }
    *slot = info;
    return true;
  };

  regData_.clear();
  boldData_.clear();
  if (!loadFile(regularTtf, &regData_)) {
    LOGE("字体加载失败：%s", regularTtf.c_str());
    return false;
  }
  if (!initFont(&regFont_, regData_)) {
    LOGE("stbtt_InitFont 失败（regular）：%s", regularTtf.c_str());
    return false;
  }

  if (!boldTtf.empty() && boldTtf != regularTtf && loadFile(boldTtf, &boldData_)) {
    if (!initFont(&boldFont_, boldData_)) {
      boldData_.clear();
      boldFont_ = nullptr;
      LOGW("粗体初始化失败，回退常规体");
    }
  } else {
    // 粗体路径缺失或与常规体相同：直接复用常规体数据（免二次读盘）
    LOGW("粗体缺失，回退常规体：%s", boldTtf.c_str());
  }
  LOGI("字体就绪：%s（regular %zu 字节）", regularTtf.c_str(), regData_.size());
  return true;
}

bool FontCache::hasCodepoint(uint32_t cp) const {
  const stbtt_fontinfo* font =
      reinterpret_cast<const stbtt_fontinfo*>(regFont_);
  if (!font) return false;
  return stbtt_FindGlyphIndex(const_cast<stbtt_fontinfo*>(font), cp) != 0;
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
  const float devSize = fontDeviceSize(size);   // 设备像素烘焙字号
  // 图集数量熔断：多次窗口缩放会累积不同缩放档的图集，超限全量释放
  //（字形懒重建，代价只有首帧几十毫秒）
  if (atlases_.size() > 32) {
    for (auto& kv : atlases_)
      if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    atlases_.clear();
  }
  Atlas& atlas = atlasFor(devSize, bold);
  bakeRenderer_ = r;
  const float s = uiscale::get();
  const auto cps = utf8ToCodepoints(utf8);
  float penX = x;
  for (uint32_t cp : cps) {
    if (cp == '\n') continue;
    const Glyph& g = bake(atlas, devSize, bold, cp);
    if (g.w > 0 && g.h > 0 && atlas.tex) {
      SDL_SetTextureColorMod(atlas.tex, color.r, color.g, color.b);
      SDL_SetTextureAlphaMod(atlas.tex, color.a);
      // 基线/偏移按设计字号；位图尺寸从设备像素折回设计单位，
      // 四边形经渲染器缩放后恰好在设备像素上 1:1 呈现
      const float baseline = y + size * 0.8f;
      const SDL_Rect src{g.px, g.py, g.w, g.h};
      const SDL_Rect dst{(int)(penX + g.xoff / s), (int)(baseline + g.yoff / s),
                         (int)(g.w / s + 0.5f), (int)(g.h / s + 0.5f)};
      SDL_RenderCopy(r, atlas.tex, &src, &dst);
    }
    penX += g.advance / s;
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
  // ★v1.2.7：按设备像素度量后折回设计单位，与 draw() 的推进量严格一致。
  stbtt_fontinfo* font =
      (bold && boldFont_) ? reinterpret_cast<stbtt_fontinfo*>(boldFont_)
                          : reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (!font) font = reinterpret_cast<stbtt_fontinfo*>(regFont_);
  if (!font) return 0;
  const float devSize = fontDeviceSize(size);
  const float scale = stbtt_ScaleForPixelHeight(font, devSize);
  const auto cps = utf8ToCodepoints(utf8);
  float w = 0;
  for (uint32_t cp : cps) {
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(font, cp, &adv, &lsb);
    w += (float)adv * scale;
  }
  return w / uiscale::get();
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
  const float devSize = fontDeviceSize(size);
  const float inv = 1.0f / uiscale::get();
  Atlas& atlas = atlasFor(devSize, bold);
  for (uint32_t cp : cps) {
    if (cp == '\n') {
      flush();
      continue;
    }
    const Glyph& g = bake(atlas, devSize, bold, cp);
    const float adv = g.advance * inv;  // 折回设计单位
    if (w + adv > maxWidth && !cur.empty()) flush();
    cur.push_back(cp);
    w += adv;
  }
  flush();
  return lines;
}

std::string FontCache::ellipsize(const std::string& utf8, float size, float maxWidth,
                                 bool bold) {
  if (textWidth(utf8, size, bold) <= maxWidth) return utf8;
  const float dots = textWidth("…", size, bold);
  const auto cps = utf8ToCodepoints(utf8);
  const float devSize = fontDeviceSize(size);
  const float inv = 1.0f / uiscale::get();
  Atlas& atlas = atlasFor(devSize, bold);
  float w = 0;
  std::vector<uint32_t> out;
  for (uint32_t cp : cps) {
    const Glyph& g = bake(atlas, devSize, bold, cp);
    const float adv = g.advance * inv;  // 折回设计单位
    if (w + adv + dots > maxWidth) break;
    out.push_back(cp);
    w += adv;
  }
  return codepointsToUtf8(out) + "…";
}

}  // namespace ns
