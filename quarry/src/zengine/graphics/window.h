#pragma once

#include "types/rectangle.h"

#include <string>

#include <SDL3/SDL_video.h>

namespace Zen {
class Window {
public:
  Window(const std::string& name, const Rectangle& window_rectangle);
  ~Window();

  SDL_Window* get_window() const;

  void set_x(int new_x) const;
  void set_y(int new_y) const;
  int get_x() const;
  int get_y() const;

  void set_width(int new_width) const;
  void set_height(int new_height);
  int get_width() const;
  int get_height() const;

private:
  SDL_Window* sdl_window;
};
}
