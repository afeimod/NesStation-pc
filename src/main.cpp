// NesStation PC — 入口
#include "app/App.h"
#include "emu/EmuRunner.h"
#include "util/Log.h"

int main(int argc, char** argv) {
  ns::App app;
  return app.run(argc, argv);
}
