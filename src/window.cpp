#include "window.h"
#include <algorithm>
#include <stdexcept>

Window::Window(const char* title, int _width, int _height):
  width(_width), height(_height) {

  int result = SDL_Init(SDL_INIT_VIDEO);
  if (result != 0) throw std::runtime_error(SDL_GetError());

  SDL_Rect usable;
  SDL_GetDisplayUsableBounds(0, &usable);
  float scale = std::min(1.0f,
    std::min((float)usable.w / width, (float)usable.h / height));

  sdlWindow = SDL_CreateWindow(title,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    (int)(width * scale), (int)(height * scale),
    SDL_WINDOW_RESIZABLE);
  if (!sdlWindow) throw std::runtime_error(SDL_GetError());

  sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
  if (!sdlRenderer) throw std::runtime_error(SDL_GetError());

  SDL_RenderSetLogicalSize(sdlRenderer, width, height);

  sdlTexture = SDL_CreateTexture(sdlRenderer,
    SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!sdlTexture) throw std::runtime_error(SDL_GetError());
}

Window::~Window() {
  SDL_DestroyTexture(sdlTexture);
  SDL_DestroyRenderer(sdlRenderer);
  SDL_DestroyWindow(sdlWindow);
  SDL_Quit();
}

Window::Event Window::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) return Window::Event::Quit;
    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) return Window::Event::Quit;
      if (event.key.keysym.sym == SDLK_s) return Window::Event::SaveImage;
    }
  }
  return Window::Event::None;
}

void Window::update(const uint8_t* pixels) {
  SDL_UpdateTexture(sdlTexture, nullptr, pixels, width * 3);
  SDL_RenderClear(sdlRenderer);
  SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, nullptr);
  SDL_RenderPresent(sdlRenderer);
}
