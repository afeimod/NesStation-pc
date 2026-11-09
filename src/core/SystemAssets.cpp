// ============================================================================
// NesStation PC — 随包核心支撑文件播种（BIOS / 密钥 / 系统档案）
// 实现说明见 SystemAssets.h 头部注释。
// ============================================================================
#include "core/SystemAssets.h"

#include <cstdlib>

#include "util/Fs.h"
#include "util/Log.h"

namespace ns::sysassets {

namespace {

using fsutil::join;

// ---- 基础原语 -------------------------------------------------------------

// 目标已存在且非空 → 保留（用户自备优先，对齐 APK “keep existing”语义）
bool copyFileIfMissing(const std::string& src, const std::string& dst) {
  if (fsutil::isFile(dst) && fsutil::fileSize(dst) > 0) return false;
  std::string data;
  if (!fsutil::readFile(src, &data)) {
    LOGW("系统档案播种：读取失败 %s", src.c_str());
    return false;
  }
  const std::string dir = fsutil::dirOf(dst);
  if (!dir.empty()) fsutil::makeDirs(dir);
  if (!fsutil::writeFile(dst, data)) {
    LOGW("系统档案播种：写入失败 %s", dst.c_str());
    return false;
  }
  return true;
}

// 目录树整体播种（递归；同样“已存在非空不覆盖”）
// ★listFilesRecursive 返回完整路径（e.fullPath）——这里换算成相对 srcDir
//   的名字，拼到目标树下（保持目录层级：sysdata/…、nand/… 原样落盘）
int copyTreeIfMissing(const std::string& srcDir, const std::string& dstDir) {
  int n = 0;
  const size_t prefix = srcDir.size() + 1;  // + 分隔符
  for (const std::string& full : fsutil::listFilesRecursive(srcDir)) {
    if (full.size() <= prefix) continue;
    const std::string rel = full.substr(prefix);
    if (copyFileIfMissing(full, join(dstDir, rel))) n++;
  }
  return n;
}

// 目录下指定扩展名（小写比较）的文件名清单（名称排序）
std::vector<std::string> filesWithExt(const std::string& dir,
                                      const std::vector<std::string>& exts) {
  std::vector<std::string> out;
  for (const auto& e : fsutil::listDir(dir)) {
    if (e.isDir) continue;
    const std::string ext = fsutil::extOf(e.name);
    for (const auto& x : exts)
      if (ext == x) {
        out.push_back(e.name);
        break;
      }
  }
  return out;
}

// ---- FDS BIOS 校验（对齐 NesApp.isValidFdsBios）----------------------------
// 真品 disksys.rom：8192 字节，复位向量（0x1FFC 小端）落在 0xE000-0xFFFF。
// 伪 ROM（NOP 填充、向量指向零页）会被 FCEUmm 接受但灰屏不启动。
bool isValidFdsBios(const std::string& path) {
  if (fsutil::fileSize(path) != 8192) return false;
  std::string data;
  if (!fsutil::readFile(path, &data) || data.size() != 8192) return false;
  const unsigned lo = (unsigned char)data[0x1FFC];
  const unsigned hi = (unsigned char)data[0x1FFD];
  const unsigned vec = (hi << 8) | lo;
  return vec >= 0xE000 && vec <= 0xFFFF;
}

// ---- 各平台播种 ------------------------------------------------------------

// FCEUmm：disksys.rom → 系统目录根（FDS 磁碟游戏必需）
int seedFds(const std::string& src, const std::string& systemDir) {
  const std::string s = join(src, "disksys.rom");
  if (!fsutil::isFile(s)) return 0;
  const std::string dst = join(systemDir, "disksys.rom");
  if (fsutil::isFile(dst) && isValidFdsBios(dst)) return 0;  // 用户已备真品
  if (copyFileIfMissing(s, dst)) {
    if (!isValidFdsBios(dst)) {
      LOGW("FDS BIOS 随包文件校验失败（复位向量非法），已移除：%s", dst.c_str());
      fsutil::removeFile(dst);
      return 0;
    }
    LOGI("FDS BIOS 已安装 → %s", dst.c_str());
    return 1;
  }
  return 0;
}

// FBNeo：neogeo.zip / pgm.zip / neocdz.zip → 系统目录/fbneo/
//（FBNeo libretro 查找顺序：system/fbneo/ → ROM 同目录 → system 根；
//  官方推荐 system/fbneo/，APK 同款 systemDir=filesDir/fbneo）
int seedFbneo(const std::string& src, const std::string& systemDir) {
  const std::string dir = join(src, "fbneo");
  if (!fsutil::isDir(dir)) return 0;
  int n = 0;
  for (const std::string& z : filesWithExt(dir, {"zip"}))
    if (copyFileIfMissing(join(dir, z), join(systemDir, "fbneo", z))) {
      LOGI("FBNeo BIOS 已安装：%s", z.c_str());
      n++;
    }
  return n;
}

// Genesis-Plus-GX：Mega-CD BIOS → 系统目录根
//（.bin 已随包预解压，核心直接可用；.zip 一并安装保持与 APK 资产一致，
//  用户在 BIOS 管理里重新导入 zip 时 PC 端没有运行时解压需求）
int seedGenesis(const std::string& src, const std::string& systemDir) {
  const std::string dir = join(src, "genesis");
  if (!fsutil::isDir(dir)) return 0;
  int n = 0;
  for (const std::string& f :
       filesWithExt(dir, {"zip", "bin"})) {
    if (f.rfind("bios_CD_", 0) != 0) continue;  // 只认规范名
    if (copyFileIfMissing(join(dir, f), join(systemDir, f))) {
      LOGI("Mega-CD BIOS 已安装：%s", f.c_str());
      n++;
    }
  }
  return n;
}

// Geargrafx：PCE-CD System Card → 系统目录根（syscard1/2/3.pce、gexpress.pce）
int seedPce(const std::string& src, const std::string& systemDir) {
  const std::string dir = join(src, "pce");
  if (!fsutil::isDir(dir)) return 0;
  int n = 0;
  for (const std::string& f : filesWithExt(dir, {"pce"})) {
    if (copyFileIfMissing(join(dir, f), join(systemDir, f))) {
      LOGI("PCE-CD System Card 已安装：%s", f.c_str());
      n++;
    }
  }
  return n;
}

// Flycast：dc_boot/dc_flash/dc_bios/dc_nvmem + naomi/awbios/机型 BIOS →
// 系统目录/dc/；VMU 模板（vmu_save_*.bin）→ 存档目录（对齐 APK：flycast
// 把 VMU 放存档目录，核心首启没有 VMU 也会自建，随包模板保证 4 槽就绪）
int seedDc(const std::string& src, const std::string& systemDir,
           const std::string& savesDir) {
  const std::string dir = join(src, "dc");
  if (!fsutil::isDir(dir)) return 0;
  int n = 0;
  for (const auto& e : fsutil::listDir(dir)) {
    if (e.isDir) continue;
    const std::string name = e.name;
    const bool isVmu = name.rfind("vmu_save_", 0) == 0;
    const std::string dstDir = isVmu ? savesDir : join(systemDir, "dc");
    if (copyFileIfMissing(join(dir, name), join(dstDir, name))) {
      LOGI("DC %s已安装：%s", isVmu ? "VMU 模板 " : "BIOS ", name.c_str());
      n++;
    }
  }
  return n;
}

// PCSX2 / LRPS2：BIOS → 系统目录/pcsx2/bios/（.md/.txt 说明文件不装）
int seedPs2(const std::string& src, const std::string& systemDir) {
  const std::string dir = join(src, join("pcsx2", "bios"));
  if (!fsutil::isDir(dir)) return 0;
  int n = 0;
  for (const auto& e : fsutil::listDir(dir)) {
    if (e.isDir) continue;
    const std::string ext = fsutil::extOf(e.name);
    if (ext == "md" || ext == "txt") continue;
    if (copyFileIfMissing(join(dir, e.name),
                          join(systemDir, join("pcsx2", "bios"), e.name))) {
      LOGI("PS2 BIOS 已安装：%s", e.name.c_str());
      n++;
    }
  }
  return n;
}

// Azahar（3DS libretro）：用户目录 = <存档目录>/Azahar/
//（azahar 源码 core_settings.cpp ParseStorageOptions：save dir + "Azahar/"，
//  与 libretro 官方文档一致；sysdata/ = aes_keys.txt + seeddb + 共享字体 +
//  CBF 字体，nand/ = Mii / 共享字体系统 title——与 APK AzaharSystemData
//  解包 Font_ACG.zip + Mii.zip 后的落盘结构完全一致）
int seedAzahar(const std::string& src, const std::string& savesDir) {
  const std::string dir = join(src, "azahar");
  if (!fsutil::isDir(dir)) return 0;
  const int n = copyTreeIfMissing(dir, join(savesDir, "Azahar"));
  if (n > 0) LOGI("Azahar 3DS 系统数据已安装 %d 项 → %s", n,
                  join(savesDir, "Azahar").c_str());
  return n;
}

// PPSSPP（PSP libretro）：原版字体 → 系统目录/PPSSPP/flash0/
//（libretro 文档：system/PPSSPP/flash0/font 替换 PPSSPP 内置字体）
int seedPpsspp(const std::string& src, const std::string& systemDir) {
  const std::string dir = join(src, join("PPSSPP", "flash0"));
  if (!fsutil::isDir(dir)) return 0;
  const int n = copyTreeIfMissing(dir, join(systemDir, join("PPSSPP", "flash0")));
  if (n > 0) LOGI("PSP 原版字体已安装 %d 项 → %s", n,
                  join(systemDir, join("PPSSPP", "flash0")).c_str());
  return n;
}

}  // namespace

void seedSystemAssets(const std::string& systemDir, const std::string& savesDir) {
  // 载荷根：环境变量 → exe 同目录 assets/system
  std::string src;
  if (const char* env = std::getenv("NESSTATION_SYSTEM_ASSETS")) {
    if (fsutil::isDir(env)) src = env;
  }
  if (src.empty()) {
    const std::string bundled =
        fsutil::join(fsutil::exeDir(), fsutil::join("assets", "system"));
    if (fsutil::isDir(bundled)) src = bundled;
  }
  if (src.empty()) {
    LOGI("系统档案：无随包载荷（assets/system 不存在），跳过播种");
    return;
  }

  // 目标目录骨架（与播种目标一致；CoreManager::init 亦建同名子目录）
  fsutil::makeDirs(join(systemDir, "fbneo"));
  fsutil::makeDirs(join(systemDir, join("pcsx2", "bios")));
  fsutil::makeDirs(join(systemDir, "dc"));
  fsutil::makeDirs(savesDir);

  int total = 0;
  total += seedFds(src, systemDir);
  total += seedFbneo(src, systemDir);
  total += seedGenesis(src, systemDir);
  total += seedPce(src, systemDir);
  total += seedDc(src, systemDir, savesDir);
  total += seedPs2(src, systemDir);
  total += seedAzahar(src, savesDir);
  total += seedPpsspp(src, systemDir);
  if (total > 0)
    LOGI("系统档案播种完成：%d 项新装（载荷 %s → 系统 %s / 存档 %s）",
         total, src.c_str(), systemDir.c_str(), savesDir.c_str());
  else
    LOGI("系统档案播种：全部就绪，无需更新（载荷 %s）", src.c_str());
}

}  // namespace ns::sysassets
