#include <SDL2/SDL.h>
#include <print>

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  std::print("Hello, SDL!");
  SDL_Quit();
  return 0;
}
