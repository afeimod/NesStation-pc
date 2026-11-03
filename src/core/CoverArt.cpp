#include "core/CoverArt.h"

#include <cstdlib>
#include <cstring>

#include "app/App.h"
#include "core/Platform.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

CoverArt::~CoverArt() {
  if (worker_.joinable()) worker_.join();
}

bool CoverArt::busy() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return workerRunning_ || !queue_.empty();
}

int CoverArt::pendingCount() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return (int)queue_.size() + (workerRunning_ ? 1 : 0);
}

std::string CoverArt::urlEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
      out.push_back((char)c);
    } else if (c == ' ') {
      out += "%20";
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    }
  }
  return out;
}

std::vector<std::string> CoverArt::candidateUrls(GamePlatform p, const std::string& titleIn) {
  std::vector<std::string> out;
  const char* sys = platformThumbnailSystem(p);
  if (!sys || titleIn.empty()) return out;

  // 文件名 → No-Intro 风格候选（原项目 nameCandidates 的简化移植）
  std::string t = titleIn;
  // 修正常见符号
  for (auto& c : t)
    if (c == '_' || c == '.') c = ' ';
  while (t.find("  ") != std::string::npos) t.replace(t.find("  "), 2, " ");
  // "The XXX" → "XXX, The"
  std::string comma;
  if (t.rfind("The ", 0) == 0) comma = t.substr(4) + ", The";
  // 去掉尾部括号标签（(Rev A) 等）作为基础名
  std::string base = t;
  const size_t paren = base.rfind(" (");
  if (paren != std::string::npos && paren > 0) base = base.substr(0, paren);

  std::vector<std::string> names;
  names.push_back(t);
  names.push_back(base);
  if (!comma.empty()) names.push_back(comma);
  const char* regions[] = {" (USA)", " (World)", " (Japan)", " (Europe)", ""};
  for (const char* r : regions) {
    names.push_back(base + r);
    if (!comma.empty()) names.push_back(comma + r);
  }

  for (const auto& n : names) {
    if (n.empty()) continue;
    const std::string enc = urlEncode(n + ".png");
    // 主站 + jsDelivr 镜像（与原项目多镜像策略一致）
    out.push_back(std::string("https://thumbnails.libretro.com/") + sys +
                  "/Named_Boxarts/" + enc);
    out.push_back(std::string("https://cdn.jsdelivr.net/gh/libretro-thumbnails/") + sys +
                  "@master/Named_Boxarts/" + enc);
  }
  return out;
}

bool CoverArt::downloadTo(const std::string& url, const std::string& dest) {
  // curl 命令（Windows 10+ 与各 Linux 发行版均预装；失败返回非零）
  const std::string tmp = dest + ".part";
#ifdef _WIN32
  const std::string cmd = "curl -sL --max-time 25 -A \"NesStationPC/1.0\" -o \"" + tmp +
                          "\" \"" + url + "\" >NUL 2>&1";
#else
  const std::string cmd = "curl -sL --max-time 25 -A 'NesStationPC/1.0' -o '" + tmp +
                          "' '" + url + "' >/dev/null 2>&1";
#endif
  const int rc = std::system(cmd.c_str());
  if (rc != 0) return false;
  // PNG 魔数校验
  FILE* f = fopen(tmp.c_str(), "rb");
  if (!f) return false;
  unsigned char magic[8] = {0};
  const size_t rd = fread(magic, 1, 8, f);
  fclose(f);
  static const unsigned char png[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (rd != 8 || std::memcmp(magic, png, 8) != 0) {
    remove(tmp.c_str());
    return false;
  }
  return std::rename(tmp.c_str(), dest.c_str()) == 0;
}

void CoverArt::requestFetch(const GameEntry& g) {
  if (!g.coverPath.empty()) return;
  std::lock_guard<std::mutex> lock(mtx_);
  for (const auto& j : queue_)
    if (j.game.id == g.id) return;
  queue_.push_back({g});
}

void CoverArt::requestFetchAllMissing(const std::vector<GameEntry>& games) {
  for (const auto& g : games) requestFetch(g);
}

void CoverArt::startWorker() {
  const std::string coverDir = coverDir_;  // 主线程快照（工作线程只读）
  worker_ = std::thread([this, coverDir]() {
    while (true) {
      Job job;
      {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) {
          workerRunning_ = false;
          return;
        }
        job = queue_.front();
        queue_.pop_front();
      }
      fsutil::makeDirs(coverDir);
      const std::string dest = fsutil::join(coverDir, job.game.id + ".png");
      if (fsutil::isFile(dest)) {
        std::lock_guard<std::mutex> lock(mtx_);
        done_.push_back({job.game.id, dest});
        continue;
      }
      bool ok = false;
      for (const auto& url : candidateUrls(job.game.platform, job.game.displayTitle())) {
        if (downloadTo(url, dest)) {
          ok = true;
          break;
        }
      }
      std::lock_guard<std::mutex> lock(mtx_);
      done_.push_back({job.game.id, ok ? dest : ""});
    }
  });
}

void CoverArt::pump(App& app) {
  if (coverDir_.empty()) coverDir_ = app.library.coverDir();

  std::vector<Done> finished;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    finished.swap(done_);
  }
  bool changed = false;
  for (const auto& d : finished) {
    if (d.coverPath.empty()) continue;
    if (GameEntry* e = app.library.find(d.gameId)) {
      if (e->coverPath.empty()) {
        e->coverPath = d.coverPath;
        changed = true;
      }
    }
  }
  if (changed) app.library.save();

  // 空闲且有任务 → 回收旧线程并启动新下载
  bool needStart = false;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    needStart = !workerRunning_ && !queue_.empty();
    if (needStart) workerRunning_ = true;
  }
  if (needStart) {
    if (worker_.joinable()) worker_.join();
    startWorker();
  }
}

}  // namespace ns
