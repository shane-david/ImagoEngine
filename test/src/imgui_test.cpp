#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

int main() {
  sf::RenderWindow window(sf::VideoMode({800, 600}), "ImGui-SFML Test");
  window.setFramerateLimit(60);

  if (!ImGui::SFML::Init(window)) {
    std::cerr << "ImGui-SFML failed to initialize" << std::endl;
    return 1;
  }

  std::cout << "ImGui-SFML is working!" << std::endl;

  sf::Clock deltaClock;
  while (window.isOpen()) {
    while (auto event = window.pollEvent()) {
      ImGui::SFML::ProcessEvent(window, *event);
      if (event->is<sf::Event::Closed>()) {
        window.close();
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());
    ImGui::Begin("Test Window");
    ImGui::Text("ImGui is working!");
    ImGui::End();

    window.clear(sf::Color::Black);
    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
  return 0;
}