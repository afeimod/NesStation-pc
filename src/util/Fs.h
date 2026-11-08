// NesStation PC — 文件系统工具（路径、目录遍历、可移植数据目录）
#pragma once
#include <cstdint>
#include <cstdio>
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

// ★字符串是否为合法 UTF-8（非法字节的文件名/目录名直接跳过：
//   ① nlohmann::json dump() 遇非法 UTF-8 会抛异常（添加目录后保存即闪退
//   的真正根因）② 界面显示也是乱码。安卓存储里 GBK 编码文件名很常见）
bool isValidUtf8(const std::string& s);

// 递归收集 romPath 指定目录下所有文件
// 深度上限 32（覆盖深层 ROM 整理目录）；符号链接环按 dev+inode 去重剪枝；
// ★单次收集上限 kMaxWalkFiles 个文件（手机/PRoot 环境扫到整块内部存储时
//   防止内存燎炸与无限等待 —— 超限即停止，已收集的照常入库）
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

// ★v1.3.0 Windows 中文路径根治：进程内字符串一律 UTF-8，所有 OS 边界
//   显式转 UTF-16。旧版 std::filesystem::path(std::string)/fopen/CreateProcessA
//   在中文系统上走 GBK（ACP）编解码：文件名返回 GBK 字节 → 界面口口乱码；
//   UTF-8 路径传给 fopen 直接失败 → 中文目录下 ROM 读取/扫描/嗅探全挂。
#ifdef _WIN32
std::wstring toWide(const std::string& utf8);   // UTF-8 → UTF-16
std::string toUtf8(const std::wstring& wide);   // UTF-16 → UTF-8
#endif

// 可移植 fopen：路径为 UTF-8（Windows 内部 _wfopen，中文路径可用）
FILE* openFile(const std::string& utf8Path, const char* mode);
// 执行 shell 命令并等待退出。Windows 下经 cmd.exe /c 但【完全无窗口】
// （★v1.3.3：旧版 _wsystem 每次弹一个 cmd 黑窗 —— 封面后台下载线程
//   对每个缺封面的游戏跑一次 curl，库一大就「一闪一闪不停弹 cmd」）。
int runCommand(const std::string& utf8Cmd);

// 同上，并捕获 stdout（替代 popen；Windows 上同样无窗口）。
// 返回退出码；stdoutOut 可为 nullptr。
int runCommandCapture(const std::string& utf8Cmd, std::string* stdoutOut);

// ★v1.3.1 用系统默认浏览器打开 URL（在线游戏 / SWF Ruffle 演示页用）。
//   Windows: start ""；macOS: open；Linux: xdg-open（缺失时返回 false）。
bool openUrl(const std::string& url);
// ★v1.3.1 ELF 动态库预检：LD_TRACE_LOADED_OBJECTS=1 枚举 exe 的依赖，
//   返回缺失的共享库名（全部可解析则返回空）。非 ELF / 检测失败返回 {"?"}
//   —— 调用方据此给出「缺运行库」而非「点了没反应」。
std::vector<std::string> missingSharedLibs(const std::string& exePath);

}  // namespace fsutil
