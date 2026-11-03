#include "util/Log.h"

#include <cstdio>
#include <ctime>
#include <mutex>

namespace ns {

namespace {
std::mutex g_logMutex;
FILE* g_logFile = nullptr;
std::string g_logPath;

const char* levelName(LogLevel lv) {
  switch (lv) {
    case LogLevel::Debug: return "D";
    case LogLevel::Info:  return "I";
    case LogLevel::Warn:  return "W";
    case LogLevel::Error: return "E";
  }
  return "?";
}
}  // namespace

void logInit(const std::string& filePath) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  if (g_logFile) return;
  g_logPath = filePath;
  g_logFile = fopen(filePath.c_str(), "ab");
  if (!g_logFile) {
    fprintf(stderr, "[NesStation] cannot open log file: %s\n", filePath.c_str());
    return;
  }
  fprintf(g_logFile, "---- NesStation PC session ----\n");
  fflush(g_logFile);
}

std::string logGetPath() { return g_logPath; }

void logClose() {
  std::lock_guard<std::mutex> lock(g_logMutex);
  if (g_logFile) {
    fclose(g_logFile);
    g_logFile = nullptr;
  }
}

void logWrite(LogLevel level, const char* tag, const char* fmt, ...) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  char timeBuf[32];
  const time_t now = time(nullptr);
  const struct tm* tmv = localtime(&now);
  if (tmv) strftime(timeBuf, sizeof(timeBuf), "%m-%d %H:%M:%S", tmv);

  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  const std::string line = std::string("[") + timeBuf + "] [" + levelName(level) + "] [" + tag + "] " + msg + "\n";
  if (g_logFile) {
    fputs(line.c_str(), g_logFile);
    fflush(g_logFile);
  } else {
    fputs(line.c_str(), stderr);
  }
}

}  // namespace ns
