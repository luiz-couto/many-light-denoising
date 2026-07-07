#include "window.h"
#include <stdexcept>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Window::Window(const char* title, int _width, int _height):
  width(_width), height(_height) {

  int result = SDL_Init(SDL_INIT_VIDEO);
  if (result != 0) throw std::runtime_error(SDL_GetError());
  
  sdlWindow = SDL_CreateWindow(
    title,
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED, 
    width, 
    height, 
    0
  );
  if (!sdlWindow) throw std::runtime_error(SDL_GetError());

  sdlRenderer = SDL_CreateRenderer(
    sdlWindow,
    -1,
    SDL_RENDERER_ACCELERATED
  );
  if (!sdlRenderer) throw std::runtime_error(SDL_GetError());

  sdlTexture = SDL_CreateTexture(
    sdlRenderer,
    SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STREAMING,
    width, 
    height
  );
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
  SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, nullptr);
  SDL_RenderPresent(sdlRenderer);
}

void Window::savePNG(const char* path, const uint8_t* pixels) {
  stbi_write_png(
    path,
    width,
    height,
    3,
    pixels,
    width * 3
  );
}
