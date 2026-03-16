#include <SFML/Graphics.hpp>
#include <iostream>
#include <windows.h>

int main() {
  // Force a console window to show any errors
  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);

  std::cout << "Starting SFML test..." << std::endl;

  sf::RenderWindow window(sf::VideoMode({800, 600}), "SFML Test");

  std::cout << "SFML window created successfully!" << std::endl;

  while (window.isOpen()) {
    while (auto event = window.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        window.close();
      }
    }
    window.clear(sf::Color::Black);
    window.display();
  }

  return 0;
}