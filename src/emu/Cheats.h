// NesStation PC — 金手指数据层（★v1.3.1）
// RetroArch .cht 格式解析 + 启用状态持久化 + 写入集汇总。
// 覆盖两种主流写法：
//   ① cheatN_code = "地址:值[,地址:值…]"（libretro 前端惯用，hex）
//   ② cheatN_address / cheatN_value（RetroArch 1.10+ 详细字段，hex）
// 其余（GameGenie 原码 / ROM 域 / 带条件比较）不在内存直写范围 → 计为「不支持」。
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "retro/LibretroCore.h"  // CheatWrite
#include "util/Fs.h"            // 中文路径安全的读写（Windows _wfopen）

namespace ns {
namespace cheats {

struct Entry {
  std::string desc;                 // cheatN_desc（缺省 “作弊码 N”）
  std::string raw;                  // 原始码文本（展示用）
  std::vector<CheatWrite> writes;   // 解析成功的内存写入（空 = 不支持格式）
  bool supported() const { return !writes.empty(); }
};

struct File {
  std::string path;  // 绝对路径
  std::string name;  // 文件名（展示）
  std::vector<Entry> entries;
};

inline std::string dir(const std::string& dataDir) {
  return dataDir + "/cheats";
}

// —— .cht 解析 ——
// 行式 INI：cheat0_desc = "xxx"（引号可选；等号两侧空白容忍）
inline File parseCht(const std::string& path) {
  File f;
  f.path = path;
  {
    const size_t slash = path.find_last_of("/\\");
    f.name = slash == std::string::npos ? path : path.substr(slash + 1);
  }
  std::string text;
  if (!fsutil::readFile(path, &text)) return f;

  // 逐行收集 cheatN_* 字段
  std::map<int, std::map<std::string, std::string>> byIdx;
  {
    size_t pos = 0;
    while (pos <= text.size()) {
      const size_t nl = text.find('\n', pos);
      std::string line = text.substr(
          pos, nl == std::string::npos ? std::string::npos : nl - pos);
      pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
      // 去注释/空白
      const size_t hash = line.find_first_of("#;");
      if (hash != std::string::npos) line = line.substr(0, hash);
      const size_t eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string key = line.substr(0, eq);
      std::string val = line.substr(eq + 1);
      auto trim = [](std::string& s) {
        size_t a = s.find_first_not_of(" \t\r");
        size_t b = s.find_last_not_of(" \t\r");
        if (a == std::string::npos) { s.clear(); return; }
        s = s.substr(a, b - a + 1);
      };
      trim(key);
      trim(val);
      if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
        val = val.substr(1, val.size() - 2);
      if (key.rfind("cheat", 0) != 0) continue;
      const size_t us = key.find('_');
      if (us == std::string::npos) continue;
      int idx = atoi(key.substr(5, us - 5).c_str());
      byIdx[idx][key.substr(us + 1)] = val;
    }
  }

  auto hexVal = [](const std::string& s, uint64_t* out) -> bool {
    if (s.empty()) return false;
    uint64_t v = 0;
    for (char c : s) {
      v <<= 4;
      if (c >= '0' && c <= '9') v |= (uint64_t)(c - '0');
      else if (c >= 'a' && c <= 'f') v |= (uint64_t)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= (uint64_t)(c - 'A' + 10);
      else return false;
    }
    *out = v;
    return true;
  };
  // “addr:value” 对 → CheatWrite（宽度按值位数：≤2 位=1 字节，3-4 位=2 字节）
  auto parsePairs = [&](const std::string& code, std::vector<CheatWrite>* out) {
    size_t pos = 0;
    while (pos <= code.size()) {
      const size_t comma = code.find(',', pos);
      const std::string seg = code.substr(
          pos, comma == std::string::npos ? std::string::npos : comma - pos);
      pos = comma == std::string::npos ? code.size() + 1 : comma + 1;
      const size_t colon = seg.find(':');
      if (colon == std::string::npos) continue;
      uint64_t a = 0, v = 0;
      if (!hexVal(seg.substr(0, colon), &a)) continue;
      const std::string vs = seg.substr(colon + 1);
      if (!hexVal(vs, &v)) continue;
      CheatWrite w;
      w.addr = a;
      w.value = (uint16_t)(v & 0xFFFF);
      w.width = vs.size() <= 2 ? 1 : 2;
      out->push_back(w);
    }
  };

  for (const auto& [idx, kv] : byIdx) {
    (void)idx;
    Entry e;
    auto it = kv.find("desc");
    e.desc = it != kv.end() && !it->second.empty() ? it->second
                                                   : "作弊码 " + std::to_string(f.entries.size() + 1);
    it = kv.find("code");
    if (it != kv.end() && !it->second.empty()) {
      e.raw = it->second;
      parsePairs(it->second, &e.writes);
    } else {
      // 详细字段形态：address + value（handler/memory_search_size 宽度提示可选）
      const auto aIt = kv.find("address");
      const auto vIt = kv.find("value");
      if (aIt != kv.end() && vIt != kv.end()) {
        uint64_t a = 0, v = 0;
        if (hexVal(aIt->second, &a) && hexVal(vIt->second, &v)) {
          e.raw = aIt->second + ":" + vIt->second;
          CheatWrite w;
          w.addr = a;
          w.value = (uint16_t)(v & 0xFFFF);
          w.width = vIt->second.size() <= 2 ? 1 : 2;
          e.writes.push_back(w);
        }
      }
    }
    if (!e.raw.empty()) f.entries.push_back(std::move(e));
  }
  return f;
}

// —— 启用状态持久化（<dataDir>/cheats/enabled.json："<文件名>#<序号>" → true）——
inline std::string enabledKey(const File& f, size_t idx) {
  return f.name + "#" + std::to_string(idx);
}

// 扫描 <dataDir>/cheats/ 下全部 .cht（CheatsScene 与 EmuRunner 启动应用共用）
inline std::vector<File> scanFiles(const std::string& dataDir) {
  std::vector<File> out;
  std::vector<std::string> found;
  fsutil::walkFiles(dir(dataDir), &found, 4);
  for (const auto& p : found) {
    if (fsutil::extOf(p) != "cht") continue;
    if (!fsutil::isValidUtf8(p)) continue;
    if (fsutil::baseOf(p) == "enabled.json") continue;
    File f = parseCht(p);
    if (!f.entries.empty()) out.push_back(std::move(f));
  }
  // 名称排序（与场景展示一致）
  for (size_t i = 1; i < out.size(); i++) {
    File key = out[i];
    size_t j = i;
    while (j > 0 && out[j - 1].name > key.name) {
      out[j] = out[j - 1];
      j--;
    }
    out[j] = std::move(key);
  }
  return out;
}

inline std::map<std::string, bool> loadEnabled(const std::string& dataDir) {
  std::map<std::string, bool> out;
  std::string text;
  if (!fsutil::readFile(dir(dataDir) + "/enabled.json", &text)) return out;
  // 轻量 JSON 提取（"key": true）
  size_t pos = 0;
  while ((pos = text.find('"', pos)) != std::string::npos) {
    const size_t q2 = text.find('"', pos + 1);
    if (q2 == std::string::npos) break;
    const std::string key = text.substr(pos + 1, q2 - pos - 1);
    const size_t colon = text.find(':', q2);
    if (colon == std::string::npos) break;
    const size_t truePos = text.find("true", colon);
    out[key] = truePos != std::string::npos && truePos < colon + 8;
    pos = text.find('"', colon);
  }
  return out;
}

inline void saveEnabled(const std::string& dataDir,
                        const std::map<std::string, bool>& enabled) {
  std::string out = "{\n";
  bool first = true;
  for (const auto& [k, v] : enabled) {
    if (!v) continue;
    if (!first) out += ",\n";
    out += "  \"" + k + "\": true";
    first = false;
  }
  out += "\n}\n";
  fsutil::writeFile(dir(dataDir) + "/enabled.json", out);
}

// 汇总全部启用条目的写入集（跨文件；EmuRunner::start 与 CheatsScene 共用）
inline std::vector<CheatWrite> activeWrites(const std::vector<File>& files,
                                            const std::map<std::string, bool>& enabled) {
  std::vector<CheatWrite> out;
  for (const auto& f : files) {
    for (size_t i = 0; i < f.entries.size(); i++) {
      auto it = enabled.find(enabledKey(f, i));
      if (it == enabled.end() || !it->second) continue;
      for (const auto& w : f.entries[i].writes) out.push_back(w);
    }
  }
  return out;
}

}  // namespace cheats
}  // namespace ns
