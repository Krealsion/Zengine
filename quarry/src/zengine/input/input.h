#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "callback.h"
#include "types/vector2.h"

namespace Zen {

class Window;

enum class TriggerType {
  PRESSED,
  RELEASED,
  TAPPED,
  HELD,
  DOUBLE_TAPPED,
  REPEAT // optional (you can ignore if not needed)
};

enum MouseButton {
  LEFT   = SDL_BUTTON_LEFT,
  MIDDLE = SDL_BUTTON_MIDDLE,
  RIGHT  = SDL_BUTTON_RIGHT,
  X1     = SDL_BUTTON_X1,
  X2     = SDL_BUTTON_X2
};

struct KeyCombo {
  SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
  std::vector<SDL_Scancode> modifiers; // canonical modifiers (sorted)

  bool operator==(const KeyCombo& other) const {
    if (key != other.key || modifiers.size() != other.modifiers.size()) return false;
    for (size_t i = 0; i < modifiers.size(); ++i) {
      if (modifiers[i] != other.modifiers[i]) return false;
    }
    return true;
  }
};

// --- Routed event payload ---
struct InputEvent {
  TriggerType type{};
  uint64_t now_ms = 0;
  int duration_ms = 0; // for RELEASED/HELD/TAPPED

  // Source can be keyboard or mouse.
  std::variant<KeyCombo, MouseButton> source{};

  // Convenience helpers
  bool is_key() const { return std::holds_alternative<KeyCombo>(source); }
  bool is_mouse() const { return std::holds_alternative<MouseButton>(source); }

  const KeyCombo& key() const { return std::get<KeyCombo>(source); }
  MouseButton mouse_button() const { return std::get<MouseButton>(source); }
};

enum class InputResult {
  PASS,     // allow lower-priority layers to handle it
  CONSUME   // stop propagation
};

// --- Safe connection token (RAII disconnect) ---
class InputConnection {
public:
  InputConnection() = default;
  InputConnection(uint64_t id, int layer_id) : _id(id), _layer_id(layer_id) {}

  InputConnection(const InputConnection&) = delete;
  InputConnection& operator=(const InputConnection&) = delete;

  InputConnection(InputConnection&& other) noexcept { *this = std::move(other); }
  InputConnection& operator=(InputConnection&& other) noexcept {
    if (this == &other) return *this;
    disconnect();
    _id = other._id;
    _layer_id = other._layer_id;
    // other_toggle = other.R_TOGGLE; What doin
    other._id = 0;
    other._layer_id = -1;
    other.R_TOGGLE = false;
    return *this;
  }

  ~InputConnection() { disconnect(); }

  void disconnect();

  explicit operator bool() const { return _id != 0; }

private:
  uint64_t _id = 0;
  int _layer_id = -1;

  // small guard if you want to disable RAII in special cases (optional)
  bool R_TOGGLE = true;

  friend class Input;
};

class Input {
public:
  // -------- Lifecycle --------
  static void init();
  static void set_active_window(Window* window);

  // Pump SDL events into raw state (no ordering logic here)
  static void update_input();

  // Provide your engine's authoritative time (ms since start in sim clock)
  // Computes edges, durations, and generates a deterministic event list for routing.
  static void step(uint64_t now_ms);

  // Routes generated events through input layers (priority order), supports consumption.
  static void route_events();

  // Runs legacy queued callbacks (optional compatibility; you can remove later)
  static void process_input_callbacks();

  // Clear per-frame mouse delta and wheel
  static void clean_input();

  // -------- Manual Polling (always available) --------
  static bool is_key_down(SDL_Scancode key);
  static bool is_key_down(const KeyCombo& combo);

  static bool was_key_pressed_this_frame(SDL_Scancode key);
  static bool was_key_released_this_frame(SDL_Scancode key);

  static int get_key_pressed_duration(SDL_Scancode key);
  static int get_key_pressed_duration(const KeyCombo& combo);

  static int get_key_released_duration(SDL_Scancode key);
  static int get_key_released_duration(const KeyCombo& combo);

  static bool is_mouse_button_down(MouseButton button);
  static bool was_mouse_pressed_this_frame(MouseButton button);
  static bool was_mouse_released_this_frame(MouseButton button);

  static Vector2 get_mouse_position();
  static Vector2 get_mouse_delta();
  static Vector2 get_mouse_wheel();
  static void clear_mouse_wheel();

  // -------- Routing Layers --------
  // Higher priority runs first. Use for overlays/modals.
  // Returns a layer id.
  static int create_layer(std::string name, int priority);

  static void set_layer_enabled(int layer_id, bool enabled);
  static void destroy_layer(int layer_id);

  // Register a handler for a specific KeyCombo + TriggerType on a layer.
  // Handler returns CONSUME to stop propagation.
  static InputConnection on_key(int layer_id,
                                TriggerType type,
                                const KeyCombo& combo,
                                std::function<InputResult(const InputEvent&)> handler,
                                int min_duration_ms = 0);

  // Register mouse handler
  static InputConnection on_mouse(int layer_id,
                                  TriggerType type,
                                  MouseButton button,
                                  std::function<InputResult(const InputEvent&)> handler,
                                  int min_duration_ms = 0);

  // A global "pre-route" filter runs before all layers. Useful for hard overrides.
  // Example: always consume ESC when overlay is open.
  static void set_global_filter(std::function<InputResult(const InputEvent&)> filter);

  // -------- Defaults & behavior knobs --------
  static void set_default_input_durations(int tap_window_ms, int double_tap_duration_ms);
  static void set_quick_tap_prevention(bool enabled, int buffer_duration_ms = 100);

  // -------- Text Input --------
  static void start_text_input();
  static void start_text_input(Action<const std::string&> callback, Action<> end_callback);
  static void end_text_input();
  static bool is_text_input_active();

  // -------- Custom Modifiers & Combo capture --------
  static void add_custom_modifier(SDL_Scancode key);
  static void remove_custom_modifier(SDL_Scancode key);
  static KeyCombo listen_for_key_combo();

  // -------- Mouse / Window --------
  static void warp_mouse(float x, float y);
  static bool warp_mouse_absolute(float x, float y);
  static void warp_mouse_to_window(Window* window, float x, float y);

  static bool show_cursor();
  static bool hide_cursor();
  static bool is_cursor_visible();
  static bool set_relative_mouse_mode(bool enabled);
  static bool get_relative_mouse_mode();

  static SDL_KeyboardID* get_keyboards(int* count);
  static const char* get_keyboard_name(SDL_KeyboardID instance_id);
  static SDL_Window* get_keyboard_focus();

  // -------- Reset --------
  static void reset_keyboard();

private:
  struct ButtonState {
    bool down = false;
    bool prev_down = false;
    bool just_pressed = false;
    bool just_released = false;

    uint64_t down_time_ms = 0;
    uint64_t up_time_ms = 0;
    uint64_t last_tap_start_ms = 0;

    // for your old “quick tap prevention” idea
    bool was_pressed_since_last_check = false;
  };

  struct MouseState {
    std::vector<ButtonState> buttons;
    float x = 0.0f, y = 0.0f;
    float dx = 0.0f, dy = 0.0f;
    float wheel_x = 0.0f, wheel_y = 0.0f;
  };

  struct RoutedHandler {
    uint64_t id = 0;
    TriggerType type{};
    std::variant<KeyCombo, MouseButton> target{};
    int min_duration_ms = 0;
    std::function<InputResult(const InputEvent&)> fn;
  };

  struct InputLayer {
    int id = -1;
    std::string name;
    int priority = 0;
    bool enabled = true;
    std::vector<RoutedHandler> handlers;
  };

  struct TriggerEventLegacy {
    TriggerType type;
    std::variant<KeyCombo, int> source; // KeyCombo or int(mouse)
    int duration_ms;
  };

private:
  static Window* _window;
  static std::mutex _mutex;

  static uint64_t _now_ms;
  static bool _has_time;

  static MouseState _mouse;

  static std::unordered_map<SDL_Scancode, ButtonState> _keys;
  static std::vector<SDL_Scancode> _pressed_scancodes;

  // Combo registry for stable identity + polling
  static std::vector<KeyCombo> _combo_list;
  static std::vector<ButtonState> _combo_states;

  static int _tap_window_ms;
  static int _double_tap_ms;

  static bool _quick_tap_prevention_enabled;
  static int _quick_tap_buffer_ms;

  // Modifier grouping
  static std::unordered_map<SDL_Scancode, int> _scancode_to_group;
  static std::unordered_map<int, SDL_Scancode> _group_to_canonical;
  static int _next_modifier_group;

  // Text input
  static bool _text_input_enabled;
  static Action<const std::string&> _update_text_input_callback;
  static Action<> _end_text_input_callback;

  // Routing
  static std::vector<InputLayer> _layers;
  static int _next_layer_id;
  static uint64_t _next_handler_id;

  static std::function<InputResult(const InputEvent&)> _global_filter;

  // Generated this frame by step(), consumed by route_events()
  static std::vector<InputEvent> _events_this_frame;

  // Optional legacy compatibility
  static std::queue<TriggerEventLegacy> _pending_triggers;

private:
  // SDL event handling (raw)
  static void _handle_event(const SDL_Event& event);
  static void _on_key_down(SDL_Scancode sc);
  static void _on_key_up(SDL_Scancode sc);
  static void _on_mouse_down(int button);
  static void _on_mouse_up(int button);
  static void _on_mouse_motion(const SDL_MouseMotionEvent& e);
  static void _on_mouse_wheel(const SDL_MouseWheelEvent& e);

  // combo + modifier math
  static ButtonState& _combo_state(const KeyCombo& combo);
  static bool _combo_matches(const KeyCombo& combo);

  // per-frame button advancement
  static void _advance(ButtonState& b);
  static int _held_ms(const ButtonState& b);
  static int _released_ms(const ButtonState& b);

  // event emission
  static void _emit_combo_events();
  static void _emit_mouse_events();

  // helper
  static InputLayer* _find_layer(int layer_id);
  static void _sort_layers_by_priority();
  static void _disconnect_handler(uint64_t handler_id, int layer_id);

  friend class InputConnection;
};

} // namespace Zen
