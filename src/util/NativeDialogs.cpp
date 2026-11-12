#include "util/NativeDialogs.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <SDL.h>
#include <SDL_syswm.h>  // SDL_GetWindowWMInfo（HWND）

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>   // CoInitializeEx / CoTaskMemFree
#include <shobjidl.h>  // IFileDialog / COMDLG_FILTERSPEC
#else
#include <cstdio>
#endif

#include "util/Fs.h"
#include "util/Log.h"

namespace nativedlg {

#ifdef _WIN32

static HWND hwndOf(void* sdlWindow) {
  if (!sdlWindow) return nullptr;
  SDL_Window* w = static_cast<SDL_Window*>(sdlWindow);
  SDL_SysWMinfo info{};
  SDL_VERSION(&info.version);
  if (SDL_GetWindowWMInfo(w, &info) == SDL_TRUE) {
#ifdef SDL_VIDEO_DRIVER_WINDOWS
    return info.info.win.window;
#endif
  }
  return nullptr;
}

static PickResult runIFileDialog(void* sdlWindow, bool pickFolder, const char* title,
                                 const wchar_t* filterSpecW, std::string* outPath) {
  // COM 初始化（本线程；已初始化场景容错）
  bool comInited = false;
  const HRESULT ci = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  comInited = SUCCEEDED(ci);
  PickResult result = PickResult::Unavailable;
  IFileDialog* dlg = nullptr;
  do {
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg)))) {
      break;
    }
    DWORD options = 0;
    dlg->GetOptions(&options);
    if (pickFolder) {
      options |= FOS_PICKFOLDERS;
    } else {
      options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    }
    options &= ~(DWORD)FOS_ALLOWMULTISELECT;
    dlg->SetOptions(options);
    if (title && *title) dlg->SetTitle(fsutil::toWide(title).c_str());
    if (!pickFolder && filterSpecW && *filterSpecW) {
      // filterSpecW = "label\0pattern\0...\0\0" 连续段 → COMDLG_FILTERSPEC 数组
      std::vector<COMDLG_FILTERSPEC> specs;
      const wchar_t* p = filterSpecW;
      while (*p) {
        const wchar_t* label = p;
        const size_t labelLen = wcslen(p);
        const wchar_t* pattern = p + labelLen + 1;
        if (!*pattern) break;
        const size_t patternLen = wcslen(pattern);
        specs.push_back({label, pattern});
        p = pattern + patternLen + 1;
      }
      if (!specs.empty()) dlg->SetFileTypes((UINT)specs.size(), specs.data());
    }
    const HRESULT hr = dlg->Show(hwndOf(sdlWindow));
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
      result = PickResult::Cancel;
      break;
    }
    if (FAILED(hr)) {
      break;
    }
    IShellItem* item = nullptr;
    if (FAILED(dlg->GetResult(&item)) || !item) {
      result = PickResult::Cancel;
      break;
    }
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
      *outPath = fsutil::toUtf8(std::wstring(path));
      CoTaskMemFree(path);
      item->Release();
      result = PickResult::Ok;
      break;
    }
    item->Release();
  } while (false);
  if (dlg) dlg->Release();
  if (comInited) CoUninitialize();
  return result;
}

// 便捷过滤串 "标签|*.ext;*.ext2|标签2|*.*" → Windows 连续段（\0 分隔，\0\0 结尾）
static std::wstring buildFilterW(const char* filterSpec) {
  if (!filterSpec || !*filterSpec) return std::wstring();
  std::wstring out;
  std::string s(filterSpec);
  size_t pos = 0;
  while (pos < s.size()) {
    const size_t bar1 = s.find('|', pos);
    const std::string label =
        s.substr(pos, bar1 == std::string::npos ? std::string::npos : bar1 - pos);
    std::string pattern;
    if (bar1 == std::string::npos) {
      pattern = "*.*";
      pos = s.size();
    } else {
      const size_t bar2 = s.find('|', bar1 + 1);
      pattern = s.substr(bar1 + 1,
                         bar2 == std::string::npos ? std::string::npos : bar2 - bar1 - 1);
      pos = bar2 == std::string::npos ? s.size() : bar2 + 1;
    }
    out += fsutil::toWide(label);
    out.push_back(L'\0');
    out += fsutil::toWide(pattern);
    out.push_back(L'\0');
  }
  out.push_back(L'\0');
  return out;
}

PickResult pickFolder(void* sdlWindow, const char* title, std::string* outPath) {
  return runIFileDialog(sdlWindow, true, title, nullptr, outPath);
}

PickResult pickFile(void* sdlWindow, const char* title, const char* filterSpec,
                    std::string* outPath) {
  const std::wstring fw = buildFilterW(filterSpec);
  return runIFileDialog(sdlWindow, false, title, fw.c_str(), outPath);
}

// ★v1.3.5 在资源管理器中打开目录（Windows）
bool openInFileManager(const std::string& dir) {
  if (dir.empty()) return false;
  const std::wstring w = fsutil::toWide(dir);
  // SW_SHOWNORMAL 打开目录本身（/select 需指向具体项）
  const INT_PTR r = (INT_PTR)ShellExecuteW(nullptr, L"open", w.c_str(), nullptr,
                                           nullptr, SW_SHOWNORMAL);
  return r > 32;
}

#else  // !_WIN32 ————————————————————————————————————————

static std::string readCommandOutput(const std::string& cmd) {
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return "";
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
  pclose(p);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
}

static bool commandExists(const char* cmd) {
  const std::string c = std::string("command -v ") + cmd + " >/dev/null 2>&1";
  return std::system(c.c_str()) == 0;
}

static std::string shellEscape(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out.push_back(c);
  }
  out += "'";
  return out;
}

static PickResult pickViaZenity(bool folder, const char* title,
                                const char* zenityFilters, std::string* outPath) {
  std::string cmd = "zenity --file-selection";
  if (folder) cmd += " --directory";
  if (title && *title) cmd += " --title=" + shellEscape(title);
  if (zenityFilters && *zenityFilters) cmd += std::string(" ") + zenityFilters;
  cmd += " 2>/dev/null";
  const std::string r = readCommandOutput(cmd);
  if (r.empty()) return PickResult::Cancel;  // zenity 存在但用户取消
  *outPath = r;
  return PickResult::Ok;
}

static PickResult pickViaKdialog(bool folder, const char* title,
                                 const char* kdialogFilter, std::string* outPath) {
  std::string cmd;
  if (folder) {
    cmd = "kdialog --getexistingdirectory $HOME";
  } else {
    cmd = "kdialog --getopenfilename $HOME " +
          (kdialogFilter ? shellEscape(kdialogFilter) : std::string("'*'"));
  }
  if (title && *title) cmd += " --title " + shellEscape(title);
  cmd += " 2>/dev/null";
  const std::string r = readCommandOutput(cmd);
  if (r.empty()) return PickResult::Cancel;
  *outPath = r;
  return PickResult::Ok;
}

// ★v1.3.5 yad（轻量 GTK 选择器；zenity 缺席时的 GTK 系后备）
static PickResult pickViaYad(bool folder, const char* title,
                             const char* yadFilters, std::string* outPath) {
  std::string cmd = "yad --file-selection";
  if (folder) cmd += " --directory";
  if (title && *title) cmd += " --title=" + shellEscape(title);
  if (yadFilters && *yadFilters) cmd += std::string(" ") + yadFilters;
  cmd += " 2>/dev/null";
  const std::string r = readCommandOutput(cmd);
  if (r.empty()) return PickResult::Cancel;
  *outPath = r;
  return PickResult::Ok;
}

// ★v1.3.5 桌面文件管理器生态探测（用户需求：「默认 thunar，没有时其他
//   文件管理器，都没有用自带」）：
//   · 文件管理器本体无法回传所选路径（thunar/nautilus/dolphin 均无
//     「选择器」模式），实际承担选择的是各桌面生态的【选择器工具】：
//       Xfce/Thunar、GNOME/Nautilus、LXDE/PCManFM → GTK 系（zenity/yad）
//       KDE/Dolphin                              → kdialog
//   · 按已装文件管理器判定生态，优先匹配该生态的选择器：
//       thunar 存在    → zenity → yad → qarma → kdialog
//       dolphin 存在   → kdialog → zenity → yad
//       其他/无法判定  → zenity → kdialog → yad
//   · 全部缺席 → PickResult::Unavailable（调用方回退内置浏览器）。
enum class FmEcosystem { Thunar, Kde, Generic };

static FmEcosystem detectEcosystem() {
  if (commandExists("thunar")) return FmEcosystem::Thunar;
  if (commandExists("dolphin") || commandExists("kdialog")) return FmEcosystem::Kde;
  return FmEcosystem::Generic;
}

struct Chooser {
  const char* tool;  // 探测用命令名
  PickResult (*fn)(bool, const char*, const char*, std::string*);
};

PickResult pickPath(bool folder, const char* title, const char* zenityFilters,
                    const char* kdialogFilter, const char* yadFilters,
                    std::string* outPath) {
  static FmEcosystem eco = detectEcosystem();  // 进程内探测一次
  std::vector<Chooser> chain;
  switch (eco) {
    case FmEcosystem::Thunar:  // Xfce：GTK 系选择器优先（zenity 的
                               // GTKFileChooserDialog 即 Thunar 同生态控件）
      chain.push_back({"zenity", pickViaZenity});
      chain.push_back({"yad", pickViaYad});
      chain.push_back({"qarma", pickViaZenity});  // qarma 兼容 zenity 参数
      chain.push_back({"kdialog", pickViaKdialog});
      break;
    case FmEcosystem::Kde:  // KDE：kdialog 优先
      chain.push_back({"kdialog", pickViaKdialog});
      chain.push_back({"zenity", pickViaZenity});
      chain.push_back({"yad", pickViaYad});
      break;
    default:
      chain.push_back({"zenity", pickViaZenity});
      chain.push_back({"kdialog", pickViaKdialog});
      chain.push_back({"yad", pickViaYad});
      break;
  }
  for (const auto& c : chain) {
    if (!commandExists(c.tool)) continue;
    const std::string f = std::string(c.tool) == "kdialog"
                              ? (kdialogFilter ? kdialogFilter : "")
                              : (std::string(c.tool) == "yad"
                                     ? (yadFilters ? yadFilters : "")
                                     : (zenityFilters ? zenityFilters : ""));
    const PickResult r =
        c.fn(folder, title, f.empty() ? nullptr : f.c_str(), outPath);
    // Ok / Cancel（用户主动关闭）都终止链 —— Cancel 不再试下一个工具
    if (r != PickResult::Unavailable) return r;
  }
  return PickResult::Unavailable;
}

PickResult pickFolder(void*, const char* title, std::string* outPath) {
  return pickPath(true, title, nullptr, nullptr, nullptr, outPath);
}

PickResult pickFile(void*, const char* title, const char* filterSpec,
                    std::string* outPath) {
  std::string zenityFilters, kdialogFilter, yadFilters;
  if (filterSpec && *filterSpec) {
    std::string s(filterSpec);
    size_t pos = 0;
    while (pos < s.size()) {
      const size_t bar1 = s.find('|', pos);
      const std::string label =
          s.substr(pos, bar1 == std::string::npos ? std::string::npos : bar1 - pos);
      std::string pattern;
      if (bar1 == std::string::npos) {
        pos = s.size();
      } else {
        const size_t bar2 = s.find('|', bar1 + 1);
        pattern = s.substr(bar1 + 1,
                           bar2 == std::string::npos ? std::string::npos : bar2 - bar1 - 1);
        pos = bar2 == std::string::npos ? s.size() : bar2 + 1;
      }
      if (pattern.empty()) continue;
      if (!zenityFilters.empty()) zenityFilters += " ";
      zenityFilters += "--file-filter=" + shellEscape(label + " | " + pattern);
      if (!kdialogFilter.empty()) kdialogFilter += " ";
      kdialogFilter += pattern;
      if (!yadFilters.empty()) yadFilters += " ";
      yadFilters += "--file-filter=" + shellEscape(label + " | " + pattern);
    }
  }
  return pickPath(false, title,
                  zenityFilters.empty() ? nullptr : zenityFilters.c_str(),
                  kdialogFilter.empty() ? nullptr : kdialogFilter.c_str(),
                  yadFilters.empty() ? nullptr : yadFilters.c_str(), outPath);
}

// ★v1.3.5 在系统文件管理器中打开目录（thunar 优先 → pcmanfm → nautilus →
//   nemo → dolphin → xdg-open；内置浏览器「在系统中打开」按钮用）。
bool openInFileManager(const std::string& dir) {
  if (dir.empty()) return false;
  static const char* kFms[] = {"thunar", "pcmanfm", "nautilus", "nemo",
                               "dolphin", nullptr};
  for (int i = 0; kFms[i]; i++) {
    if (!commandExists(kFms[i])) continue;
    const std::string cmd = std::string(kFms[i]) + " " + shellEscape(dir) +
                            " >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
  }
  const std::string cmd = "xdg-open " + shellEscape(dir) + " >/dev/null 2>&1 &";
  return std::system(cmd.c_str()) == 0;
}

#endif  // _WIN32

}  // namespace nativedlg
