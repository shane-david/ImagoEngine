#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
    std::cerr << "SDL2 failed to initialize: " << SDL_GetError() << std::endl;
    return 1;
  }

  std::cout << "SDL2 is working!" << std::endl;
  std::cout << "Number of joysticks: " << SDL_NumJoysticks() << std::endl;

  SDL_Quit();
  return 0;
}