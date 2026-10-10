#include "input/input.h"

#include "graphics/window.h"
#include "state_management/game_state_manager.h"

namespace Zen {

Window* Input::_window = nullptr;
std::mutex Input::_mutex;

uint64_t Input::_now_ms = 0;
bool Input::_has_time = false;

Input::MouseState Input::_mouse;

std::unordered_map<SDL_Scancode, Input::ButtonState> Input::_keys;
std::vector<SDL_Scancode> Input::_pressed_scancodes;

std::vector<KeyCombo> Input::_combo_list;
std::vector<Input::ButtonState> Input::_combo_states;

int Input::_tap_window_ms = 200;
int Input::_double_tap_ms = 300;

bool Input::_quick_tap_prevention_enabled = true;
int Input::_quick_tap_buffer_ms = 100;

std::unordered_map<SDL_Scancode, int> Input::_scancode_to_group;
std::unordered_map<int, SDL_Scancode> Input::_group_to_canonical;
int Input::_next_modifier_group = 0;

bool Input::_text_input_enabled = false;
Action<const std::string&> Input::_update_text_input_callback = Action<const std::string&>();
Action<> Input::_end_text_input_callback = Action<>();

std::vector<Input::InputLayer> Input::_layers;
int Input::_next_layer_id = 1;
uint64_t Input::_next_handler_id = 1;

std::function<InputResult(const InputEvent&)> Input::_global_filter = nullptr;

std::vector<InputEvent> Input::_events_this_frame;

std::queue<Input::TriggerEventLegacy> Input::_pending_triggers;

static bool contains_scancode(const std::vector<SDL_Scancode>& v, SDL_Scancode sc) {
  return std::find(v.begin(), v.end(), sc) != v.end();
}

void InputConnection::disconnect() {
  if (!_id || !_layer_id || _layer_id < 0) return;
  if (!R_TOGGLE) return;
  Input::_disconnect_handler(_id, _layer_id);
  _id = 0;
  _layer_id = -1;
}

void Input::init() {
  std::lock_guard lock(_mutex);

  _mouse.buttons.resize(5);

  SDL_SetGamepadEventsEnabled(true);

  // Standard modifier groups
  int shift_group  = _next_modifier_group++;
  int ctrl_group   = _next_modifier_group++;
  int alt_group    = _next_modifier_group++;
  int gui_group    = _next_modifier_group++;
  int caps_group   = _next_modifier_group++;
  int num_group    = _next_modifier_group++;
  int scroll_group = _next_modifier_group++;

  _scancode_to_group[SDL_SCANCODE_LSHIFT] = shift_group;
  _scancode_to_group[SDL_SCANCODE_RSHIFT] = shift_group;
  _group_to_canonical[shift_group] = SDL_SCANCODE_LSHIFT;

  _scancode_to_group[SDL_SCANCODE_LCTRL] = ctrl_group;
  _scancode_to_group[SDL_SCANCODE_RCTRL] = ctrl_group;
  _group_to_canonical[ctrl_group] = SDL_SCANCODE_LCTRL;

  _scancode_to_group[SDL_SCANCODE_LALT] = alt_group;
  _scancode_to_group[SDL_SCANCODE_RALT] = alt_group;
  _group_to_canonical[alt_group] = SDL_SCANCODE_LALT;

  _scancode_to_group[SDL_SCANCODE_LGUI] = gui_group;
  _scancode_to_group[SDL_SCANCODE_RGUI] = gui_group;
  _group_to_canonical[gui_group] = SDL_SCANCODE_LGUI;

  _scancode_to_group[SDL_SCANCODE_CAPSLOCK] = caps_group;
  _group_to_canonical[caps_group] = SDL_SCANCODE_CAPSLOCK;

  _scancode_to_group[SDL_SCANCODE_NUMLOCKCLEAR] = num_group;
  _group_to_canonical[num_group] = SDL_SCANCODE_NUMLOCKCLEAR;

  _scancode_to_group[SDL_SCANCODE_SCROLLLOCK] = scroll_group;
  _group_to_canonical[scroll_group] = SDL_SCANCODE_SCROLLLOCK;

  _layers.clear();
  _events_this_frame.clear();
}

void Input::set_active_window(Window* window) {
  std::lock_guard lock(_mutex);
  _window = window;
}

void Input::update_input() {
  std::lock_guard lock(_mutex);

  SDL_PumpEvents();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      GameStateManager::singleton().exit();
      continue;
    }

    // Text input mode routing
    if (_text_input_enabled) {
      if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_RETURN) {
          if (_update_text_input_callback) _update_text_input_callback("\0");
          end_text_input();
          continue;
        }
        if (event.key.key == SDLK_BACKSPACE) {
          if (_update_text_input_callback) _update_text_input_callback("\b");
          continue;
        }
      } else if (event.type == SDL_EVENT_TEXT_INPUT) {
        if (_update_text_input_callback) _update_text_input_callback(event.text.text);
        continue;
      } else {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          end_text_input();
          // fallthrough: also handle click as normal input
        }
      }
    }

    _handle_event(event);
  }
}

void Input::step(uint64_t now_ms) {
  std::lock_guard lock(_mutex);

  _now_ms = now_ms;
  _has_time = true;

  _events_this_frame.clear();

  // advance key edge flags
  for (auto& [sc, b] : _keys) _advance(b);

  // advance combo edge flags + emit events
  _emit_combo_events();

  // advance mouse edge flags + emit events
  _emit_mouse_events();
}

void Input::route_events() {
  std::lock_guard lock(_mutex);

  // Highest priority first
  _sort_layers_by_priority();

  for (const InputEvent& e : _events_this_frame) {
    // global pre-filter (optional hard override)
    if (_global_filter) {
      if (_global_filter(e) == InputResult::CONSUME) continue;
    }

    for (auto& layer : _layers) {
      if (!layer.enabled) continue;

      // layer handlers run in registration order
      for (const auto& h : layer.handlers) {
        if (h.type != e.type) continue;

        // match target
        if (std::holds_alternative<KeyCombo>(h.target)) {
          if (!e.is_key()) continue;
          if (!(std::get<KeyCombo>(h.target) == e.key())) continue;
        } else {
          if (!e.is_mouse()) continue;
          if (std::get<MouseButton>(h.target) != e.mouse_button()) continue;
        }

        // duration gate
        if (h.min_duration_ms > 0 && e.duration_ms < h.min_duration_ms) continue;

        // invoke
        InputResult r = InputResult::PASS;
        if (h.fn) r = h.fn(e);

        if (r == InputResult::CONSUME) {
          goto next_event;
        }
      }
    }

  next_event:
    continue;
  }
}

void Input::process_input_callbacks() {
  // Keep this if you still want your older queued callback system.
  // You can also delete this entirely once you migrate to layers.
  std::lock_guard lock(_mutex);
  while (!_pending_triggers.empty()) {
    _pending_triggers.pop();
  }
}

void Input::clean_input() {
  std::lock_guard lock(_mutex);
  _mouse.dx = 0.0f;
  _mouse.dy = 0.0f;
  _mouse.wheel_x = 0.0f;
  _mouse.wheel_y = 0.0f;
}

bool Input::is_key_down(SDL_Scancode key) {
  return is_key_down(KeyCombo{key, {}});
}

bool Input::is_key_down(const KeyCombo& combo) {
  std::lock_guard lock(_mutex);
  ButtonState& cs = _combo_state(combo);

  // combo.down updated during step(); if step hasn’t run yet, compute now
  bool down_now = cs.down;
  if (!_has_time) down_now = _combo_matches(combo);

  if (!down_now && _quick_tap_prevention_enabled && _has_time && cs.was_pressed_since_last_check) {
    const uint64_t since_up = (_now_ms >= cs.up_time_ms) ? (_now_ms - cs.up_time_ms) : 9999999;
    if (since_up <= (uint64_t)_quick_tap_buffer_ms) down_now = true;
  }

  cs.was_pressed_since_last_check = false;
  return down_now;
}

bool Input::was_key_pressed_this_frame(SDL_Scancode key) {
  std::lock_guard lock(_mutex);
  auto it = _keys.find(key);
  if (it == _keys.end()) return false;
  return it->second.just_pressed;
}

bool Input::was_key_released_this_frame(SDL_Scancode key) {
  std::lock_guard lock(_mutex);
  auto it = _keys.find(key);
  if (it == _keys.end()) return false;
  return it->second.just_released;
}

int Input::get_key_pressed_duration(SDL_Scancode key) {
  return get_key_pressed_duration(KeyCombo{key, {}});
}

int Input::get_key_pressed_duration(const KeyCombo& combo) {
  std::lock_guard lock(_mutex);
  if (!_has_time) return -1;
  ButtonState& cs = _combo_state(combo);
  if (is_key_down(combo)) return _held_ms(cs);
  return -1;
}

int Input::get_key_released_duration(SDL_Scancode key) {
  return get_key_released_duration(KeyCombo{key, {}});
}

int Input::get_key_released_duration(const KeyCombo& combo) {
  std::lock_guard lock(_mutex);
  if (!_has_time) return -1;
  ButtonState& cs = _combo_state(combo);
  if (!is_key_down(combo)) return _released_ms(cs);
  return -1;
}

bool Input::is_mouse_button_down(MouseButton button) {
  std::lock_guard lock(_mutex);
  int idx = (int)button - 1;
  if (idx < 0 || idx >= (int)_mouse.buttons.size()) return false;
  ButtonState& b = _mouse.buttons[idx];

  bool down_now = b.down;

  if (!down_now && _quick_tap_prevention_enabled && _has_time && b.was_pressed_since_last_check) {
    const uint64_t since_up = (_now_ms >= b.up_time_ms) ? (_now_ms - b.up_time_ms) : 9999999;
    if (since_up <= (uint64_t)_quick_tap_buffer_ms) down_now = true;
  }

  b.was_pressed_since_last_check = false;
  return down_now;
}

bool Input::was_mouse_pressed_this_frame(MouseButton button) {
  std::lock_guard lock(_mutex);
  int idx = (int)button - 1;
  if (idx < 0 || idx >= (int)_mouse.buttons.size()) return false;
  return _mouse.buttons[idx].just_pressed;
}

bool Input::was_mouse_released_this_frame(MouseButton button) {
  std::lock_guard lock(_mutex);
  int idx = (int)button - 1;
  if (idx < 0 || idx >= (int)_mouse.buttons.size()) return false;
  return _mouse.buttons[idx].just_released;
}

Vector2 Input::get_mouse_position() {
  std::lock_guard lock(_mutex);
  return {_mouse.x, _mouse.y};
}

Vector2 Input::get_mouse_delta() {
  std::lock_guard lock(_mutex);
  return {_mouse.dx, _mouse.dy};
}

Vector2 Input::get_mouse_wheel() {
  std::lock_guard lock(_mutex);
  return {_mouse.wheel_x, _mouse.wheel_y};
}

void Input::clear_mouse_wheel() {
  std::lock_guard lock(_mutex);
  _mouse.wheel_x = 0.0f;
  _mouse.wheel_y = 0.0f;
}

int Input::create_layer(std::string name, int priority) {
  std::lock_guard lock(_mutex);
  InputLayer l;
  l.id = _next_layer_id++;
  l.name = std::move(name);
  l.priority = priority;
  l.enabled = true;
  _layers.push_back(std::move(l));
  _sort_layers_by_priority();
  return _layers.back().id;
}

void Input::set_layer_enabled(int layer_id, bool enabled) {
  std::lock_guard lock(_mutex);
  if (auto* l = _find_layer(layer_id)) l->enabled = enabled;
}

void Input::destroy_layer(int layer_id) {
  std::lock_guard lock(_mutex);
  _layers.erase(std::remove_if(_layers.begin(), _layers.end(),
                               [&](const InputLayer& l){ return l.id == layer_id; }),
                _layers.end());
}

InputConnection Input::on_key(int layer_id,
                              TriggerType type,
                              const KeyCombo& combo,
                              std::function<InputResult(const InputEvent&)> handler,
                              int min_duration_ms) {
  std::lock_guard lock(_mutex);
  InputLayer* l = _find_layer(layer_id);
  if (!l) return {};

  // ensure combo state exists for polling + event emission
  (void)_combo_state(combo);

  RoutedHandler h;
  h.id = _next_handler_id++;
  h.type = type;
  h.target = combo;
  h.min_duration_ms = min_duration_ms;
  h.fn = std::move(handler);

  l->handlers.push_back(std::move(h));
  return InputConnection{l->handlers.back().id, layer_id};
}

InputConnection Input::on_mouse(int layer_id,
                                TriggerType type,
                                MouseButton button,
                                std::function<InputResult(const InputEvent&)> handler,
                                int min_duration_ms) {
  std::lock_guard lock(_mutex);
  InputLayer* l = _find_layer(layer_id);
  if (!l) return {};

  RoutedHandler h;
  h.id = _next_handler_id++;
  h.type = type;
  h.target = button;
  h.min_duration_ms = min_duration_ms;
  h.fn = std::move(handler);

  l->handlers.push_back(std::move(h));
  return InputConnection{l->handlers.back().id, layer_id};
}

void Input::set_global_filter(std::function<InputResult(const InputEvent&)> filter) {
  std::lock_guard lock(_mutex);
  _global_filter = std::move(filter);
}

void Input::set_default_input_durations(int tap_window_ms, int double_tap_duration_ms) {
  std::lock_guard lock(_mutex);
  _tap_window_ms = tap_window_ms;
  _double_tap_ms = double_tap_duration_ms;
}

void Input::set_quick_tap_prevention(bool enabled, int buffer_duration_ms) {
  std::lock_guard lock(_mutex);
  _quick_tap_prevention_enabled = enabled;
  _quick_tap_buffer_ms = buffer_duration_ms;
}

void Input::start_text_input() {
  std::lock_guard lock(_mutex);
  if (_window) SDL_StartTextInput(_window->get_window());
  _text_input_enabled = true;
}

void Input::start_text_input(Action<const std::string&> callback, Action<> end_callback) {
  std::lock_guard lock(_mutex);
  _update_text_input_callback = callback;
  _end_text_input_callback = end_callback;
  if (_window) SDL_StartTextInput(_window->get_window());
  _text_input_enabled = true;
}

void Input::end_text_input() {
  std::lock_guard lock(_mutex);
  if (_window) SDL_StopTextInput(_window->get_window());
  const auto cb = _end_text_input_callback;

  _text_input_enabled = false;
  _update_text_input_callback = Action<const std::string&>();
  _end_text_input_callback = Action<>();

  if (cb) cb();
}

bool Input::is_text_input_active() {
  std::lock_guard lock(_mutex);
  return _text_input_enabled;
}

void Input::add_custom_modifier(SDL_Scancode key) {
  std::lock_guard lock(_mutex);
  if (!_scancode_to_group.contains(key)) {
    int group = _next_modifier_group++;
    _scancode_to_group[key] = group;
    _group_to_canonical[group] = key;
  }
}

void Input::remove_custom_modifier(SDL_Scancode key) {
  std::lock_guard lock(_mutex);
  auto it = _scancode_to_group.find(key);
  if (it != _scancode_to_group.end()) {
    int group = it->second;
    _scancode_to_group.erase(it);
    _group_to_canonical.erase(group);
  }
}

KeyCombo Input::listen_for_key_combo() {
  std::vector<SDL_Scancode> temp_pressed;
  SDL_Event event;
  while (true) {
    SDL_WaitEvent(&event);
    if (event.type == SDL_EVENT_KEY_DOWN) {
      SDL_Scancode sc = event.key.scancode;
      if (!contains_scancode(temp_pressed, sc)) temp_pressed.push_back(sc);
    } else if (event.type == SDL_EVENT_KEY_UP) {
      SDL_Scancode sc = event.key.scancode;
      auto it = std::find(temp_pressed.begin(), temp_pressed.end(), sc);
      if (it != temp_pressed.end()) temp_pressed.erase(it);

      if (!_scancode_to_group.contains(sc)) {
        std::unordered_set<int> pressed_groups;
        for (SDL_Scancode p : temp_pressed) {
          auto git = _scancode_to_group.find(p);
          if (git != _scancode_to_group.end()) pressed_groups.insert(git->second);
        }
        std::vector<SDL_Scancode> mods;
        for (int g : pressed_groups) mods.push_back(_group_to_canonical[g]);
        std::sort(mods.begin(), mods.end());
        return {sc, mods};
      }
    }
  }
}

void Input::warp_mouse(float x, float y) {
  std::lock_guard lock(_mutex);
  if (_window) SDL_WarpMouseInWindow(_window->get_window(), x, y);
}

bool Input::warp_mouse_absolute(float x, float y) {
  return SDL_WarpMouseGlobal(x, y);
}

void Input::warp_mouse_to_window(Window* window, float x, float y) {
  if (window) SDL_WarpMouseInWindow(window->get_window(), x, y);
}

bool Input::show_cursor() { return SDL_ShowCursor(); }
bool Input::hide_cursor() { return SDL_HideCursor(); }
bool Input::is_cursor_visible() { return SDL_CursorVisible(); }

bool Input::set_relative_mouse_mode(bool enabled) {
  std::lock_guard lock(_mutex);
  if (!_window) return false;
  return SDL_SetWindowRelativeMouseMode(_window->get_window(), enabled);
}

bool Input::get_relative_mouse_mode() {
  std::lock_guard lock(_mutex);
  if (!_window) return false;
  return SDL_GetWindowRelativeMouseMode(_window->get_window());
}

SDL_KeyboardID* Input::get_keyboards(int* count) { return SDL_GetKeyboards(count); }
const char* Input::get_keyboard_name(SDL_KeyboardID instance_id) { return SDL_GetKeyboardNameForID(instance_id); }
SDL_Window* Input::get_keyboard_focus() { return SDL_GetKeyboardFocus(); }

void Input::reset_keyboard() {
  std::lock_guard lock(_mutex);
  SDL_ResetKeyboard();
  _keys.clear();
  _pressed_scancodes.clear();
  _combo_list.clear();
  _combo_states.clear();
  _events_this_frame.clear();
}

void Input::_handle_event(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN: _on_key_down(event.key.scancode); break;
    case SDL_EVENT_KEY_UP:   _on_key_up(event.key.scancode); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: _on_mouse_down(event.button.button); break;
    case SDL_EVENT_MOUSE_BUTTON_UP:   _on_mouse_up(event.button.button); break;
    case SDL_EVENT_MOUSE_MOTION: _on_mouse_motion(event.motion); break;
    case SDL_EVENT_MOUSE_WHEEL:  _on_mouse_wheel(event.wheel); break;
    default: break;
  }
}

void Input::_on_key_down(SDL_Scancode sc) {
  if (!contains_scancode(_pressed_scancodes, sc)) _pressed_scancodes.push_back(sc);

  ButtonState& b = _keys[sc];
  b.down = true;
  b.was_pressed_since_last_check = true;
}

void Input::_on_key_up(SDL_Scancode sc) {
  auto it = std::find(_pressed_scancodes.begin(), _pressed_scancodes.end(), sc);
  if (it != _pressed_scancodes.end()) _pressed_scancodes.erase(it);

  ButtonState& b = _keys[sc];
  b.down = false;
}

void Input::_on_mouse_down(int button) {
  int idx = button - 1;
  if (idx < 0 || idx >= (int)_mouse.buttons.size()) return;
  _mouse.buttons[idx].down = true;
  _mouse.buttons[idx].was_pressed_since_last_check = true;
}

void Input::_on_mouse_up(int button) {
  int idx = button - 1;
  if (idx < 0 || idx >= (int)_mouse.buttons.size()) return;
  _mouse.buttons[idx].down = false;
}

void Input::_on_mouse_motion(const SDL_MouseMotionEvent& e) {
  _mouse.x = e.x;
  _mouse.y = e.y;
  _mouse.dx += e.xrel;
  _mouse.dy += e.yrel;
}

void Input::_on_mouse_wheel(const SDL_MouseWheelEvent& e) {
  if (e.direction == SDL_MOUSEWHEEL_FLIPPED) {
    _mouse.wheel_x -= e.x;
    _mouse.wheel_y -= e.y;
  } else {
    _mouse.wheel_x += e.x;
    _mouse.wheel_y += e.y;
  }
}

Input::ButtonState& Input::_combo_state(const KeyCombo& combo) {
  for (size_t i = 0; i < _combo_list.size(); ++i) {
    if (_combo_list[i] == combo) return _combo_states[i];
  }
  KeyCombo c = combo;
  std::sort(c.modifiers.begin(), c.modifiers.end());

  _combo_list.push_back(c);
  _combo_states.emplace_back(ButtonState{});
  return _combo_states.back();
}

bool Input::_combo_matches(const KeyCombo& combo) {
  // Snapshot pressed groups + pressed set
  std::unordered_set<int> pressed_groups;
  std::unordered_set<SDL_Scancode> pressed_set(_pressed_scancodes.begin(), _pressed_scancodes.end());

  for (SDL_Scancode sc : _pressed_scancodes) {
    auto it = _scancode_to_group.find(sc);
    if (it != _scancode_to_group.end()) pressed_groups.insert(it->second);
  }

  // Key
  if (auto key_it = _scancode_to_group.find(combo.key); key_it != _scancode_to_group.end()) {
    if (!pressed_groups.contains(key_it->second)) return false;
  } else {
    if (!pressed_set.contains(combo.key)) return false;
  }

  // Modifiers
  for (SDL_Scancode mod : combo.modifiers) {
    if (auto mod_it = _scancode_to_group.find(mod); mod_it != _scancode_to_group.end()) {
      if (!pressed_groups.contains(mod_it->second)) return false;
    } else {
      if (!pressed_set.contains(mod)) return false;
    }
  }

  return true;
}

void Input::_advance(ButtonState& b) {
  b.just_pressed  = (!b.prev_down && b.down);
  b.just_released = ( b.prev_down && !b.down);

  if (_has_time) {
    if (b.just_pressed)  b.down_time_ms = _now_ms;
    if (b.just_released) b.up_time_ms = _now_ms;
  }

  b.prev_down = b.down;
}

int Input::_held_ms(const ButtonState& b) {
  if (!_has_time) return 0;
  if (b.down) return (int)(_now_ms - b.down_time_ms);
  if (b.up_time_ms >= b.down_time_ms) return (int)(b.up_time_ms - b.down_time_ms);
  return 0;
}

int Input::_released_ms(const ButtonState& b) {
  if (!_has_time) return 0;
  if (b.down) return 0;
  return (int)(_now_ms - b.up_time_ms);
}

void Input::_emit_combo_events() {
  // Update each combo state "down" from match, then edge-detect, then emit
  for (size_t i = 0; i < _combo_list.size(); ++i) {
    ButtonState& cs = _combo_states[i];

    cs.prev_down = cs.down;
    cs.down = _combo_matches(_combo_list[i]);
    _advance(cs);

    if (cs.just_pressed) {
      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::PRESSED,
        .now_ms = _now_ms,
        .duration_ms = 0,
        .source = _combo_list[i]
      });

      // double tap detection
      if (cs.last_tap_start_ms > 0 && (_now_ms - cs.last_tap_start_ms) < (uint64_t)_double_tap_ms) {
        _events_this_frame.push_back(InputEvent{
          .type = TriggerType::DOUBLE_TAPPED,
          .now_ms = _now_ms,
          .duration_ms = 0,
          .source = _combo_list[i]
        });
        cs.last_tap_start_ms = 0;
      } else {
        cs.last_tap_start_ms = _now_ms;
      }

      cs.was_pressed_since_last_check = true;
    }

    if (cs.just_released) {
      const int duration = _held_ms(cs);

      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::RELEASED,
        .now_ms = _now_ms,
        .duration_ms = duration,
        .source = _combo_list[i]
      });

      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::HELD,
        .now_ms = _now_ms,
        .duration_ms = duration,
        .source = _combo_list[i]
      });

      if (duration < _tap_window_ms) {
        _events_this_frame.push_back(InputEvent{
          .type = TriggerType::TAPPED,
          .now_ms = _now_ms,
          .duration_ms = duration,
          .source = _combo_list[i]
        });
      }
    }
  }
}

void Input::_emit_mouse_events() {
  for (int i = 0; i < (int)_mouse.buttons.size(); ++i) {
    ButtonState& b = _mouse.buttons[i];
    _advance(b);

    if (b.just_pressed) {
      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::PRESSED,
        .now_ms = _now_ms,
        .duration_ms = 0,
        .source = (MouseButton)(i + 1)
      });
    }

    if (b.just_released) {
      const int duration = _held_ms(b);

      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::RELEASED,
        .now_ms = _now_ms,
        .duration_ms = duration,
        .source = (MouseButton)(i + 1)
      });

      _events_this_frame.push_back(InputEvent{
        .type = TriggerType::HELD,
        .now_ms = _now_ms,
        .duration_ms = duration,
        .source = (MouseButton)(i + 1)
      });

      if (duration < _tap_window_ms) {
        _events_this_frame.push_back(InputEvent{
          .type = TriggerType::TAPPED,
          .now_ms = _now_ms,
          .duration_ms = duration,
          .source = (MouseButton)(i + 1)
        });
      }
    }
  }
}

Input::InputLayer* Input::_find_layer(int layer_id) {
  for (auto& l : _layers) if (l.id == layer_id) return &l;
  return nullptr;
}

void Input::_sort_layers_by_priority() {
  std::sort(_layers.begin(), _layers.end(),
            [](const InputLayer& a, const InputLayer& b) {
              // higher first
              return a.priority > b.priority;
            });
}

void Input::_disconnect_handler(uint64_t handler_id, int layer_id) {
  std::lock_guard lock(_mutex);
  InputLayer* l = _find_layer(layer_id);
  if (!l) return;

  l->handlers.erase(std::remove_if(l->handlers.begin(), l->handlers.end(),
                                  [&](const RoutedHandler& h){ return h.id == handler_id; }),
                    l->handlers.end());
}

} // namespace Zen
