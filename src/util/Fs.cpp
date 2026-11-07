#include "util/Fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>   // SHGetFolderPathW / CSIDL_*（中文用户目录）
#include <wchar.h>    // _wsystem / 宽字符函数
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "util/Log.h"

namespace fsutil {
namespace {
namespace stdfs = std::filesystem;

std::string g_dataDirOverride;

std::string normalizeSep(std::string s) {
  for (auto& c : s)
    if (c == '\\') c = '/';
  return s;
}

#ifdef _WIN32
// Windows 统一入口：UTF-8 字符串 → UTF-16 的 std::filesystem::path。
// stdfs::path(std::string) 在 MSVC 按 ACP（中文系统 = GBK）解码，绝不能用。
std::filesystem::path widenPath(const std::string& utf8) {
  if (utf8.empty()) return std::filesystem::path();
  return std::filesystem::path(toWide(utf8));
}
#endif

}  // namespace

// —— ★v1.3.0 UTF-8 ↔ UTF-16（Windows 专用）——————————————————————

#ifdef _WIN32
std::wstring toWide(const std::string& utf8) {
  if (utf8.empty()) return std::wstring();
  const int need = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(),
                                       nullptr, 0);
  if (need <= 0) return std::wstring();
  std::wstring out((size_t)need, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &out[0], need);
  return out;
}

std::string toUtf8(const std::wstring& wide) {
  if (wide.empty()) return std::string();
  const int need = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(),
                                       nullptr, 0, nullptr, nullptr);
  if (need <= 0) return std::string();
  std::string out((size_t)need, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &out[0], need,
                      nullptr, nullptr);
  return out;
}
#endif

FILE* openFile(const std::string& utf8Path, const char* mode) {
#ifdef _WIN32
  // mode 是 ASCII，直接逐字符扩宽
  std::wstring wmode;
  for (const char* p = mode; p && *p; ++p) wmode.push_back((wchar_t)*p);
  return _wfopen(toWide(utf8Path).c_str(), wmode.c_str());
#else
  return fopen(utf8Path.c_str(), mode);
#endif
}

int runCommand(const std::string& utf8Cmd) {
#ifdef _WIN32
  // _wsystem：宽字符命令行 → cmd 正确处理中文路径（curl -o 落盘 / 解压等）
  return _wsystem(toWide(utf8Cmd).c_str());
#else
  return std::system(utf8Cmd.c_str());
#endif
}

// 带符号环去重的递归枚举实现（walkFiles 用）
void walkFilesImpl(const std::string& dir, std::vector<std::string>* out, int maxDepth,
                   int depth, std::set<std::pair<uint64_t, uint64_t>>* visited);

std::string join(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  std::string r = a;
  if (r.back() != '/' && r.back() != '\\') r += '/';
  return r + b;
}

std::string join(const std::string& a, const std::string& b, const std::string& c) {
  return join(join(a, b), c);
}

// —— 路径切分：统一在 UTF-8 字符串上按 ASCII 分隔符操作（中文安全）——
static void splitPath(const std::string& path, std::string* dirOut,
                      std::string* baseOut) {
  const size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    if (dirOut) *dirOut = "";
    if (baseOut) *baseOut = path;
    return;
  }
  if (dirOut) *dirOut = path.substr(0, pos);
  if (baseOut) *baseOut = path.substr(pos + 1);
}

std::string dirOf(const std::string& path) {
  // Windows 盘根特殊处理："D:/" 的父目录是它自身（文件浏览器顶层语义）
  std::string d;
  splitPath(path, &d, nullptr);
  if (d.empty()) return ".";
#ifdef _WIN32
  if (d.size() == 2 && d[1] == ':') d += "/";  // "D:" → "D:/"
#endif
  return d;
}

std::string baseOf(const std::string& path) {
  std::string b;
  splitPath(path, nullptr, &b);
  return b;
}

std::string stemOf(const std::string& path) {
  const std::string b = baseOf(path);
  const size_t dot = b.find_last_of('.');
  if (dot == std::string::npos || dot == 0) return b;
  return b.substr(0, dot);
}

std::string extOf(const std::string& path) {
  const std::string b = baseOf(path);
  const size_t dot = b.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= b.size()) return "";
  std::string e = b.substr(dot + 1);
  std::transform(e.begin(), e.end(), e.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return e;
}

bool exists(const std::string& path) {
  std::error_code ec;
#ifdef _WIN32
  return stdfs::exists(widenPath(path), ec);
#else
  return stdfs::exists(path, ec);
#endif
}
bool isDir(const std::string& path) {
  std::error_code ec;
#ifdef _WIN32
  return stdfs::is_directory(widenPath(path), ec);
#else
  return stdfs::is_directory(path, ec);
#endif
}
bool isFile(const std::string& path) {
  std::error_code ec;
#ifdef _WIN32
  return stdfs::is_regular_file(widenPath(path), ec);
#else
  return stdfs::is_regular_file(path, ec);
#endif
}

bool canReadDir(const std::string& path) {
#ifdef _WIN32
  std::error_code ec;
  return stdfs::is_directory(widenPath(path), ec);  // Windows 枚举不受 POSIX R_OK 语义限制
#else
  struct ::stat st;
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
         ::access(path.c_str(), R_OK) == 0;
#endif
}

bool makeDirs(const std::string& path) {
  if (path.empty()) return false;
  std::error_code ec;
#ifdef _WIN32
  return stdfs::create_directories(widenPath(path), ec) ||
         stdfs::is_directory(widenPath(path), ec);
#else
  return stdfs::create_directories(path, ec) || stdfs::is_directory(path, ec);
#endif
}

bool readFile(const std::string& path, std::string* out) {
#ifdef _WIN32
  std::ifstream f(widenPath(path), std::ios::binary);
#else
  std::ifstream f(path, std::ios::binary);
#endif
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  *out = ss.str();
  return true;
}

bool writeFile(const std::string& path, const std::string& data) {
#ifdef _WIN32
  std::ofstream f(widenPath(path), std::ios::binary | std::ios::trunc);
#else
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
#endif
  if (!f) return false;
  f.write(data.data(), (std::streamsize)data.size());
  return f.good();
}

bool removeFile(const std::string& path) {
  std::error_code ec;
#ifdef _WIN32
  return stdfs::remove(widenPath(path), ec);
#else
  return stdfs::remove(path, ec);
#endif
}

uint64_t fileSize(const std::string& path) {
  std::error_code ec;
#ifdef _WIN32
  const auto sz = stdfs::file_size(widenPath(path), ec);
#else
  const auto sz = stdfs::file_size(path, ec);
#endif
  return ec ? 0 : (uint64_t)sz;
}

// ★UTF-8 合法性校验（严格 RFC 3629：拒超长/代理区/孤立连续字节）
bool isValidUtf8(const std::string& s) {
  const auto* p = (const unsigned char*)s.data();
  const size_t n = s.size();
  size_t i = 0;
  while (i < n) {
    const unsigned char c = p[i];
    int len; unsigned int cp;
    if (c < 0x80) { i++; continue; }
    else if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
    else return false;  // 0x80-0xBF 孤立连续字节 / 0xF8+
    if (i + (size_t)len > n) return false;  // 截断
    for (int k = 1; k < len; k++) {
      if ((p[i + k] & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (p[i + k] & 0x3F);
    }
    if (len == 2 && cp < 0x80) return false;           // 超长编码
    if (len == 3 && cp < 0x800) return false;
    if (len == 4 && cp < 0x10000) return false;
    if (cp > 0x10FFFF) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;    // 代理区
    i += (size_t)len;
  }
  return true;
}

std::vector<DirEntry> listDir(const std::string& dir) {
  std::vector<DirEntry> out;
#ifdef _WIN32
  // ★v1.3.0：FindFirstFileW + UTF-16→UTF-8（旧版 stdfs::path(string) 在中文
  //   系统按 GBK 解码：目录含中文打不开；文件名返回 GBK 字节 → 界面口口乱码）
  const std::wstring pattern = toWide(join(dir, "*"));
  if (pattern.empty()) return out;
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    const std::string name = toUtf8(std::wstring(fd.cFileName));
    if (name.empty() || name == "." || name == "..") continue;
    DirEntry e;
    e.name = name;
    e.fullPath = join(dir, name);
    e.isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!e.isDir) {
      e.size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;
    }
    out.push_back(std::move(e));
  } while (FindNextFileW(h, &fd));
  FindClose(h);
#else
  // Linux / macOS：纯 POSIX opendir/readdir（含 d_type 未知回退 stat，
  // 各 arm64 发行版 / 老工具链下最稳，避开 libstdc++ filesystem 兼容性问题）
  DIR* d = opendir(dir.c_str());
  if (!d) return out;
  struct dirent* de;
  while ((de = readdir(d)) != nullptr) {
    const char* n = de->d_name;
    if (n[0] == 0 || (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0)))) continue;
    DirEntry e;
    e.name = n;
    e.fullPath = join(dir, e.name);
    bool knownDir = false, knownFile = false;
#ifdef DT_DIR
    if (de->d_type == DT_DIR) knownDir = true;
    else if (de->d_type == DT_REG) knownFile = true;
#endif
    if (!knownDir && !knownFile) {
      // d_type 未知（部分文件系统 / 老内核）：stat 兜底
      struct stat st;
      if (::stat(e.fullPath.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) knownDir = true;
        else if (S_ISREG(st.st_mode)) knownFile = true;
      } else {
        continue;  // 无法识别（坏链接 / 权限）→ 跳过该项
      }
    }
    e.isDir = knownDir;
    e.size = 0;
    if (knownFile) {
      struct stat st;
      if (::stat(e.fullPath.c_str(), &st) == 0) e.size = (uint64_t)st.st_size;
    }
    out.push_back(std::move(e));
  }
  closedir(d);
#endif
  std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
    if (a.isDir != b.isDir) return a.isDir;  // 目录优先
    return normalizeSep(a.name) < normalizeSep(b.name);
  });
  return out;
}

void walkFiles(const std::string& dir, std::vector<std::string>* out, int maxDepth,
               int depth) {
  walkFilesImpl(dir, out, maxDepth, depth, nullptr);
}

// 带符号链接环去重的内部实现（dev+ino 记录已访问目录）
// ★单次扫描文件数上限：防手机/PRoot 环境扫到整块内部存储时内存燎炸
constexpr size_t kMaxWalkFiles = 20000;
void walkFilesImpl(const std::string& dir, std::vector<std::string>* out, int maxDepth,
                   int depth, std::set<std::pair<uint64_t, uint64_t>>* visitedOuter) {
  if (depth > maxDepth) return;
  if (out && out->size() >= kMaxWalkFiles) return;  // 超限即停（防 OOM）
  std::set<std::pair<uint64_t, uint64_t>> local;
  std::set<std::pair<uint64_t, uint64_t>>& visited = visitedOuter ? *visitedOuter : local;
#ifndef _WIN32
  // 目录本身 dev+ino 入表：符号链接指回祖先目录时立即剪枝
  {
    struct stat st;
    if (::stat(dir.c_str(), &st) == 0) {
      const auto key = std::make_pair((uint64_t)st.st_dev, (uint64_t)st.st_ino);
      if (!visited.insert(key).second) return;  // 已访问（符号环）
    }
  }
#endif
  for (const auto& e : listDir(dir)) {
    if (out && out->size() >= kMaxWalkFiles) return;  // 超限即停
    if (e.isDir) {
      try {
        walkFilesImpl(e.fullPath, out, maxDepth, depth + 1, &visited);
      } catch (...) {
        // 单个子目录异常不影响整体扫描
      }
    } else {
      out->push_back(e.fullPath);
    }
  }
}

std::vector<std::string> listFilesRecursive(const std::string& dir) {
  std::vector<std::string> out;
  walkFiles(dir, &out);
  return out;
}

std::string exeDir() {
#ifdef _WIN32
  // ★v1.3.0：GetModuleFileNameW + UTF-8 转换（exe 装在中文目录也能正确解析）
  wchar_t buf[MAX_PATH * 4] = {0};
  GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]) - 1));
  return dirOf(toUtf8(std::wstring(buf)));
#else
  std::error_code ec;
  const auto p = stdfs::read_symlink("/proc/self/exe", ec);
  if (!ec) return p.parent_path().string();
  return ".";
#endif
}

std::string homeDir() {
#ifdef _WIN32
  // ★v1.3.0：中文用户名目录（C:\Users\张三）正确返回 UTF-8
  wchar_t buf[MAX_PATH * 4] = {0};
  if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buf) == S_OK) {
    const std::string p = toUtf8(std::wstring(buf));
    if (!p.empty()) return p;
  }
  const char* p = std::getenv("USERPROFILE");
  if (p && *p) return p;
  const char* hd = std::getenv("HOMEDRIVE");
  const char* hp = std::getenv("HOMEPATH");
  if (hd && hp && *hd && *hp) return std::string(hd) + hp;
  return "";
#else
  const char* h = std::getenv("HOME");
  return (h && *h) ? h : "";
#endif
}

std::vector<std::string> storageMounts() {
  std::vector<std::string> out;
#ifdef _WIN32
  // ★v1.3.0：Windows 返回全部盘符（A-Z 存在的）—— 旧版整个函数体在
  //   #ifndef _WIN32 里，Windows 恒返回空 → 文件浏览器只剩 C 盘入口
  const DWORD mask = GetLogicalDrives();
  for (int i = 0; i < 26; i++) {
    if (!(mask & (1u << i))) continue;
    const std::string root = std::string(1, (char)('A' + i)) + ":/";
    if (isDir(root)) out.push_back(root);
  }
  return out;
#else
  auto push = [&](const std::string& p) {
    if (p.empty() || !isDir(p)) return;
    for (const auto& e : out)
      if (e == p) return;
    out.push_back(p);
  };
  // Android Linux 环境（Termux / proot 等）
  push("/storage/emulated/0");
  push("/sdcard");
  push("/storage");
  // 桌面 Linux：U 盘 / SD 卡自动挂载
  for (const char* base : {"/media", "/run/media", "/mnt"}) {
    if (!isDir(base)) continue;
    for (const auto& e : listDir(base)) {
      if (!e.isDir) continue;
      const std::string& p = e.fullPath;
      // /run/media/<user>/<vol> 需再下一层
      bool hasChild = false;
      for (const auto& c : listDir(p)) {
        if (c.isDir) {
          push(c.fullPath);
          hasChild = true;
        }
      }
      if (!hasChild) push(p);
    }
  }
  push("/data");  // Termux 环境共享数据
  return out;
#endif
}

std::string dataDir() {
  if (!g_dataDirOverride.empty()) return g_dataDirOverride;
  // 便携模式：exe 同目录 portable.flag
  const std::string portable = join(exeDir(), "portable.flag");
  if (isFile(portable)) return join(exeDir(), "UserData");

#ifdef _WIN32
  // ★v1.3.0：SHGetFolderPathW（中文用户名正确；旧版 getenv("APPDATA")
  //   在非 Unicode 环境变量下可能返回 GBK）
  {
    wchar_t buf[MAX_PATH * 4] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK) {
      const std::string p = toUtf8(std::wstring(buf));
      if (!p.empty()) return join(p, "NesStation");
    }
  }
  const char* appdata = std::getenv("APPDATA");
  if (appdata && *appdata) return join(appdata, "NesStation");
  return join(exeDir(), "UserData");
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg) return join(xdg, "NesStation");
  const char* home = std::getenv("HOME");
  if (home && *home) return join(join(home, ".config"), "NesStation");
  return join(exeDir(), "UserData");
#endif
}

void setDataDirOverrideForTests(const std::string& dir) { g_dataDirOverride = dir; }

std::string pathDisplay(const std::string& path) {
#ifdef _WIN32
  return path;  // Windows 资源管理器习惯
#else
  return normalizeSep(path);
#endif
}

}  // namespace fsutil
