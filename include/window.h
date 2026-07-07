#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>
#include <cstdint>


class Window {
  public:
  int width, height;
  enum class Event { None, Quit, SaveImage };
  
  Window(const char* title, int width, int height);
  ~Window();
  
  // Returns Event
  Event pollEvents();
  
  // Upload RGB buffer and present
  void update(const uint8_t* pixels); 
  
  // Save pixels as a PNG file
  void savePNG(const char* path, const uint8_t* pixels);
  
private:
  SDL_Window* sdlWindow;
  SDL_Renderer* sdlRenderer;
  SDL_Texture* sdlTexture;
};

#endif // WINDOW_H
