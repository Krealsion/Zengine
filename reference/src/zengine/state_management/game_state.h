#pragma once

#include "callback.h"
#include "game_state_manager.h"

namespace Zen {
class GameStateManager;
class CustomLayout;

class GameState {
friend class GameStateManager;

public:
  GameState();
  virtual ~GameState();
  virtual void update() = 0;
  virtual void draw(GameGraphics& g) = 0;

  virtual void exit();
  virtual void pause() {}
  virtual void resume() {}

  Window* get_window() {
    return get_renderer()->get_window();
  }
  Renderer* get_renderer() {
    return GameStateManager::singleton().get_renderer();
  }

  void add_overlay(CustomLayout* overlay) {
    _overlays.push_back(overlay);
  }

  void add_mouse_callback(const Callback<bool, Vector2>& callback, CustomLayout* layout_attachment) {
    _mouse_callbacks.emplace_back(callback, layout_attachment);
  }

  void remove_overlay(CustomLayout* overlay) {
    std::erase(_overlays, overlay);
    std::erase_if(_mouse_callbacks, [overlay](const auto& callback_tuple) { return std::get<1>(callback_tuple) == overlay; });
  }

protected:
  CustomLayout* _root_layout = nullptr;
  std::vector<CustomLayout*> _overlays;
  std::vector<std::tuple<Callback<bool, Vector2>, CustomLayout*>> _mouse_callbacks; // bool is if the input is consumed, Vector2 is the clicked location
private:
  void draw_gamestate(GameGraphics& g);
};
}

