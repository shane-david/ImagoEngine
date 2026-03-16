#include <spdlog/spdlog.h>

int main() {
  spdlog::info("spdlog is working!");
  spdlog::warn("this is a warning");
  spdlog::error("this is an error");
  return 0;
}