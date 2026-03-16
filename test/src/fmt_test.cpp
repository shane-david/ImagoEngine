#include <fmt/core.h>

int main() {
  fmt::print("fmt is working!\n");
  fmt::print("formatted number: {}\n", 42);
  fmt::print("formatted float: {:.2f}\n", 3.14159);
  return 0;
}
