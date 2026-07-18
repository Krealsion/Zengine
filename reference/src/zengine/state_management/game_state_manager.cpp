#include "game_state_manager.h"

#include "input/input.h"
#include "game_state.h"
#include "logger.h"
#include "timer.h"
#include "../../config_manager.h"
#include "graphics/texture_manager.h"

#include "ui/custom_layout.h"

namespace Zen {

void GameStateManager::initialize_with_state(GameState* initial_state) {
  if (initial_state == nullptr) {
    throw std::runtime_error("Initial game state cannot be null");
  }
  push_state(initial_state);
  _running = true;
  // _update_thread = std::thread([&]() {
  // });
//  _draw_thread = std::thread([&]() {
//    while (is_running())
//  });
  while (_running) {
    Input::update_input();
    ConfigManager::instance().reload_if_changed();
    update();
    draw();
    Input::clean_input();
  }
  // _update_thread.join();
//  _draw_thread.join();
}

GameState* GameStateManager::get_current_state() const {
  return _game_states.back();
}

GameStateManager::GameStateManager() : _renderer("", Rectangle(0, 0, 1820, 980)) {
  Input::init();
  TextureManager::init_ttf();
  _running = true;
}

GameStateManager::~GameStateManager() {
  while (!_game_states.empty()) {
    _game_states.back()->exit();
    delete _game_states.back();
    _game_states.pop_back();
  }
}

void GameStateManager::update() {
  Timer::update_time();
  const KeyCombo AltF4 = {SDL_SCANCODE_F4, {SDL_SCANCODE_LALT}};
  if (Input::is_key_down(AltF4)) {
    exit(); // Exit on Alt + F4
  }
  if (!_game_states.empty()) {
    if (Input::is_mouse_button_down(LEFT) && !_mouse_captured) {
      bool consumed = false;
      for (const auto& callback_tuple : _game_states.back()->_mouse_callbacks) {
        if (auto callback = std::get<0>(callback_tuple); callback(Input::get_mouse_position())) {
          consumed = true;
          break;
        }
      }
      if (!consumed) {
        _game_states.back()->_root_layout->click_location(Input::get_mouse_position());
      }
      _mouse_captured = true;
    }
    if (!Input::is_mouse_button_down(LEFT)) {
      _mouse_captured = false;
    }
    _game_states.back()->update();
    for (const auto& overlay : _game_states.back()->_overlays) {
      overlay->update();
    }
    _game_states.back()->_root_layout->update();
  }
}

void GameStateManager::draw() {
  if (!_game_states.empty()) {
    _game_states.back()->draw_gamestate(_graphics);
  }
  _renderer.render_game_graphics(_graphics);
}

void GameStateManager::push_state(GameState* state) {
  if (!_game_states.empty()) {
    _game_states.back()->pause();
  }
  _game_states.push_back(state);
}

void GameStateManager::pop_state() {
  if (!_game_states.empty()) {
    const GameState* back = _game_states.back();
    _game_states.pop_back();
    delete back;
    if (!_game_states.empty()) {
      _game_states.back()->resume();
    }
  }
}

void GameStateManager::exit() {
  _running = false;
}

Renderer* GameStateManager::get_renderer() {
  return &_renderer;
}
}
