// NesStation PC — 封面艺术：本地封面 / libretro 官方缩略图库自动获取
//
// 移植自 Android 版 CoverFetcher（thumbnails.libretro.com + jsDelivr 镜像 +
// 候选文件名），网络下载经 curl 命令（Windows 10+ / Linux 系统自带），
// 工作线程落盘，主线程 pump() 合并结果回库。
#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/GameLibrary.h"

namespace ns {

class App;

class CoverArt {
 public:
  ~CoverArt();

  // 请求为游戏抓取封面（异步；已有本地封面则忽略）
  void requestFetch(const GameEntry& g);
  // 批量：库内所有缺封面的游戏
  void requestFetchAllMissing(const std::vector<GameEntry>& games);
  // 每帧调用：完成项回写库 + 启动下一个下载
  void pump(App& app);
  bool busy() const;
  int pendingCount() const;

 private:
  struct Job {
    GameEntry game;
  };
  struct Done {
    std::string gameId;
    std::string coverPath;  // 空 = 失败
  };

  void startWorker();
  static bool downloadTo(const std::string& url, const std::string& dest);
  // libretro 缩略图候选 URL 列表（名称变体 + 镜像）
  static std::vector<std::string> candidateUrls(GamePlatform p, const std::string& title);
  static std::string urlEncode(const std::string& s);

  std::deque<Job> queue_;
  std::vector<Done> done_;
  mutable std::mutex mtx_;
  std::thread worker_;
  bool workerRunning_ = false;
  std::string coverDir_;
};

}  // namespace ns
