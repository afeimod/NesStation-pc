// NesStation PC — 字体渲染（stb_truetype 按需烘焙字形图集，支持中日韩）
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>

#include "ui/Ui.h"

namespace ns {

class FontCache {
 public:
  ~FontCache();

  bool load(const std::string& regularTtf, const std::string& boldTtf);

  // 字体是否包含某码点字形（App 字体选链用：校验中文字形 U+4E2D）
  bool hasCodepoint(uint32_t cp) const;

  // 绘制一行 UTF-8 文本（左上角锚点），返回文本宽度
  float draw(SDL_Renderer* r, const std::string& utf8, float x, float y, float size,
             Color8 color, bool bold = false);
  // 垂直居中绘制（y 为中心）
  float drawVCentered(SDL_Renderer* r, const std::string& utf8, float x, float cy,
                      float size, Color8 color, bool bold = false);

  float textWidth(const std::string& utf8, float size, bool bold = false);
  float lineHeight(float size) const;

  // 按最大宽度折行
  std::vector<std::string> wrap(const std::string& utf8, float size, float maxWidth,
                                bool bold = false);

  // 截断到 maxWidth 并加省略号
  std::string ellipsize(const std::string& utf8, float size, float maxWidth,
                        bool bold = false);

 private:
  struct Glyph {
    // 归一化 UV + 像素坐标（源矩形用）
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    int px = 0, py = 0;
    int w = 0, h = 0;           // 位图尺寸
    float xoff = 0, yoff = 0;   // 相对基线
    float advance = 0;
  };
  struct Atlas {
    SDL_Texture* tex = nullptr;
    int size = 512;
    int cursorX = 1, cursorY = 1, rowH = 0;
    std::map<uint32_t, Glyph> glyphs;
  };
  Atlas& atlasFor(float size, bool bold);
  const Glyph& bake(Atlas& atlas, float size, bool bold, uint32_t cp);
  bool atlasPlace(Atlas& atlas, int w, int h);  // 放置游标推进（false=需扩容）

  // stbtt 字体数据
  std::vector<uint8_t> regData_, boldData_;
  void* regFont_ = nullptr;  // stbtt_fontinfo*（前向声明避免头泄漏）
  void* boldFont_ = nullptr;
  std::map<uint64_t, Atlas> atlases_;  // key = size*2+bold
  SDL_Renderer* bakeRenderer_ = nullptr;
};

// UTF-8 工具
std::vector<uint32_t> utf8ToCodepoints(const std::string& s);
size_t utf8Len(const std::string& s);

}  // namespace ns
