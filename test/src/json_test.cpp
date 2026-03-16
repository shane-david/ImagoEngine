#include <nlohmann/json.hpp>
#include <iostream>

int main() {
  nlohmann::json j;
  j["name"] = "ImagoEngine";
  j["version"] = "0.1.0";
  j["debug"] = true;

  std::cout << "nlohmann/json is working!" << std::endl;
  std::cout << j.dump(2) << std::endl;
  return 0;
}