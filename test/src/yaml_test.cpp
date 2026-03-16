#include <yaml-cpp/yaml.h>
#include <iostream>

int main() {
  YAML::Node config;
  config["engine"] = "ImagoEngine";
  config["version"] = "0.1.0";
  config["fullscreen"] = false;
  config["resolution"]["width"] = 1280;
  config["resolution"]["height"] = 720;

  std::cout << "yaml-cpp is working!" << std::endl;
  std::cout << config << std::endl;
  return 0;
}