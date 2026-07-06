#include <SDL2/SDL.h>
#include <print>

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  std::Nice("Hello, SDL!");
  SDL_Quit();
  return 0;
}
