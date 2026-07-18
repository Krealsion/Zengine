#pragma once

#include "graphics/game_graphics.h"
#include "graphics/renderer.h"

#include <vector>
#include <thread>

namespace Zen {

class GameState;

/**
 * TODO for thread saftey, ensure that draw does not draw null objects by having an itterator
 * On gamestate pop, all objects will be deleted, possibly partway through a render
 * Do not complete draw, and continue as normal
 * Atomic bool needed
 * Before draw finish
 */
class GameStateManager {
public:

  //singleton
  static GameStateManager& singleton() {
    static GameStateManager instance;
    return instance;
  }
  GameStateManager(const GameStateManager&) = delete;
  GameStateManager& operator=(const GameStateManager&) = delete;

  void initialize_with_state(GameState* initial_state);

  GameState* get_current_state() const;

  GameStateManager();
  ~GameStateManager();

  void push_state(GameState* state);
  void pop_state();
  void exit();

  Renderer* get_renderer();

protected:
  void update();
  void draw();

  std::thread _update_thread;
  std::thread _draw_thread;
  bool _running;
  bool _mouse_captured = false;
  Renderer _renderer;
  GameGraphics _graphics;
  std::vector<GameState*> _game_states;
};
}

