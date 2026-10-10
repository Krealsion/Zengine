#pragma once

#include "game_graphics.h"
#include "types/rectangle.h"
#include "window.h"


#include <vector>
#include <string>
#include <map>

namespace Zen {
/**
 * The renderer is in charge of creating and displaying the window
 * as well as any rendering of objects to the window
 */
 /**
  * The engine should be able to handle multiple windows at once
  * each renderer needs its own window
  */
class Renderer {
public:
  Renderer(const std::string& name, const Rectangle& window_rectangle);
  ~Renderer();

  void render_game_graphics(GameGraphics& game_graphics) const;

  Window* get_window();

protected:
  Window _game_window;
  SDL_Renderer* _renderer;
};
}
