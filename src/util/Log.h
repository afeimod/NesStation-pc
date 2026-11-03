// NesStation PC — 日志工具（文件 + 控制台）
#pragma once
#include <cstdarg>
#include <cstdint>
#include <string>

namespace ns {

enum class LogLevel : int { Debug = 0, Info = 1, Warn = 2, Error = 3 };

#if defined(_MSC_VER)
#define NS_PRINTF_FORMAT(fmtIdx, vaIdx)
#else
#define NS_PRINTF_FORMAT(fmtIdx, vaIdx) __attribute__((format(printf, fmtIdx, vaIdx)))
#endif

// 初始化日志（写 <dataDir>/nesstation.log）。可安全重入。
void logInit(const std::string& filePath);
void logWrite(LogLevel level, const char* tag, const char* fmt, ...) NS_PRINTF_FORMAT(3, 4);
void logClose();

std::string logGetPath();

}  // namespace ns

#define LOGD(...) ::ns::logWrite(::ns::LogLevel::Debug, "NSS", __VA_ARGS__)
#define LOGI(...) ::ns::logWrite(::ns::LogLevel::Info, "NSS", __VA_ARGS__)
#define LOGW(...) ::ns::logWrite(::ns::LogLevel::Warn, "NSS", __VA_ARGS__)
#define LOGE(...) ::ns::logWrite(::ns::LogLevel::Error, "NSS", __VA_ARGS__)
