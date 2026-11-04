// NesStation PC — 文件系统工具（路径、目录遍历、可移植数据目录）
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fsutil {

// 目录项
struct DirEntry {
  std::string name;        // 文件/目录名
  std::string fullPath;    // 绝对/完整路径
  bool isDir = false;
  uint64_t size = 0;       // 字节（文件）
};

// 分隔符规范化（全部接受 '/' 与系统分隔符）
std::string join(const std::string& a, const std::string& b);
std::string join(const std::string& a, const std::string& b, const std::string& c);
std::string dirOf(const std::string& path);            // 父目录
std::string baseOf(const std::string& path);           // 文件名
std::string stemOf(const std::string& path);           // 去扩展名的文件名
std::string extOf(const std::string& path);            // 小写扩展名（无点）
bool exists(const std::string& path);
bool isDir(const std::string& path);
bool isFile(const std::string& path);
// 目录是否可读（区分「空目录」与「无权限」，文件浏览器提示用）
bool canReadDir(const std::string& path);
bool makeDirs(const std::string& path);                // 递归建目录
bool readFile(const std::string& path, std::string* out);
bool writeFile(const std::string& path, const std::string& data);
bool removeFile(const std::string& path);
uint64_t fileSize(const std::string& path);
std::vector<DirEntry> listDir(const std::string& dir); // 名称排序，目录优先
std::vector<std::string> listFilesRecursive(const std::string& dir);

// 递归收集 romPath 指定目录下所有文件
// 深度上限 32（覆盖深层 ROM 整理目录）；符号链接环按 dev+inode 去重剪枝
void walkFiles(const std::string& dir, std::vector<std::string>* out,
               int maxDepth = 32, int depth = 0);

// 执行文件所在目录（用于 exe 同目录的 cores/、emulators/、assets/ 探测）
std::string exeDir();

// 用户主目录（HOME / USERPROFILE；取不到返回空串）
std::string homeDir();

// 可移动/外置存储位置（Linux：/media/*、/run/media/*/*、/mnt/*、/storage/*、
// /sdcard、/data；Windows 返回空）。文件浏览器“存储位置”书签用。
std::vector<std::string> storageMounts();

// 用户数据目录：<config>/NesStation；便携模式（exe 旁 portable.flag）则为
// <exeDir>/UserData
std::string dataDir();
void setDataDirOverrideForTests(const std::string& dir);

std::string pathDisplay(const std::string& path);      // 路径展示（统一分隔符）

}  // namespace fsutil
