#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

int main() {
  glm::vec2 position(100.0f, 200.0f);
  glm::vec2 velocity(1.5f, 2.5f);
  glm::vec2 newPosition = position + velocity;

  std::cout << "glm is working!" << std::endl;
  std::cout << "position: " << newPosition.x << ", " << newPosition.y << std::endl;
  return 0;
}