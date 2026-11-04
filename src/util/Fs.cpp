#include "util/Fs.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
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
}  // namespace

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

std::string dirOf(const std::string& path) {
  std::error_code ec;
  const stdfs::path p(path);
  const stdfs::path parent = p.parent_path();
  return parent.empty() ? std::string(".") : parent.string();
}

std::string baseOf(const std::string& path) {
  return stdfs::path(path).filename().string();
}

std::string stemOf(const std::string& path) {
  return stdfs::path(path).stem().string();
}

std::string extOf(const std::string& path) {
  std::string e = stdfs::path(path).extension().string();
  if (!e.empty() && e[0] == '.') e.erase(0, 1);
  std::transform(e.begin(), e.end(), e.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return e;
}

bool exists(const std::string& path) {
  std::error_code ec;
  return stdfs::exists(path, ec);
}
bool isDir(const std::string& path) {
  std::error_code ec;
  return stdfs::is_directory(path, ec);
}
bool isFile(const std::string& path) {
  std::error_code ec;
  return stdfs::is_regular_file(path, ec);
}

bool canReadDir(const std::string& path) {
#ifdef _WIN32
  std::error_code ec;
  return stdfs::is_directory(path, ec);  // Windows 枚举不受 POSIX R_OK 语义限制
#else
  struct ::stat st;
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
         ::access(path.c_str(), R_OK) == 0;
#endif
}

bool makeDirs(const std::string& path) {
  if (path.empty()) return false;
  std::error_code ec;
  return stdfs::create_directories(path, ec) || stdfs::is_directory(path, ec);
}

bool readFile(const std::string& path, std::string* out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  *out = ss.str();
  return true;
}

bool writeFile(const std::string& path, const std::string& data) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(data.data(), (std::streamsize)data.size());
  return f.good();
}

bool removeFile(const std::string& path) {
  std::error_code ec;
  return stdfs::remove(path, ec);
}

uint64_t fileSize(const std::string& path) {
  std::error_code ec;
  const auto sz = stdfs::file_size(path, ec);
  return ec ? 0 : (uint64_t)sz;
}

std::vector<DirEntry> listDir(const std::string& dir) {
  std::vector<DirEntry> out;
  std::error_code ec;
  stdfs::path p(dir);
  if (!stdfs::is_directory(p, ec)) return out;
  for (stdfs::directory_iterator it(p, ec), end; it != end && !ec; it.increment(ec)) {
    DirEntry e;
    e.name = it->path().filename().string();
    if (e.name.empty() || e.name == "." || e.name == "..") continue;
    e.fullPath = it->path().string();
    std::error_code ec2;
    e.isDir = it->is_directory(ec2);
    e.size = e.isDir ? 0 : fileSize(e.fullPath);
    out.push_back(std::move(e));
  }
  std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
    if (a.isDir != b.isDir) return a.isDir;  // 目录优先
    return normalizeSep(a.name) < normalizeSep(b.name);
  });
  return out;
}

void walkFiles(const std::string& dir, std::vector<std::string>* out, int maxDepth, int depth) {
  if (depth > maxDepth) return;
  for (const auto& e : listDir(dir)) {
    if (e.isDir) {
      walkFiles(e.fullPath, out, maxDepth, depth + 1);
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
  char buf[MAX_PATH * 4] = {0};
  GetModuleFileNameA(nullptr, buf, sizeof(buf) - 1);
  return dirOf(std::string(buf));
#else
  std::error_code ec;
  const auto p = stdfs::read_symlink("/proc/self/exe", ec);
  if (!ec) return p.parent_path().string();
  return ".";
#endif
}

std::string homeDir() {
#ifdef _WIN32
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
#ifndef _WIN32
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
#endif
  return out;
}

std::string dataDir() {
  if (!g_dataDirOverride.empty()) return g_dataDirOverride;
  // 便携模式：exe 同目录 portable.flag
  const std::string portable = join(exeDir(), "portable.flag");
  if (isFile(portable)) return join(exeDir(), "UserData");

#ifdef _WIN32
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
