// NesStation PC — 资源查找（字体等；安装布局与便携布局兼容）
#include <string>

#include "util/Fs.h"

namespace ns {
namespace res {

// 资源根目录：exe/assets → exe/../share/nesstation/assets
std::string assetsDir() {
  const std::string exe = fsutil::exeDir();
  if (fsutil::isDir(fsutil::join(exe, "assets"))) return fsutil::join(exe, "assets");
  const std::string share =
      fsutil::join(fsutil::dirOf(exe), "share/nesstation/assets");
  if (fsutil::isDir(share)) return share;
  return fsutil::join(exe, "assets");
}

std::string font(const std::string& name) { return fsutil::join(assetsDir(), "fonts/" + name); }

}  // namespace res
}  // namespace ns
