#pragma once

#include "state_management/game_state.h"
#include "ui/custom_layout.h"
#include "ui/button.h"
#include "ui/text.h"
#include "ui/scroll_view.h"
#include "input/input.h"

#include "cookie_view.h"

#include <string>
#include <vector>

#include "../../../src/engine/timer.h"

namespace Zen {

class CookieClickerState : public GameState {
public:
  CookieClickerState();
  ~CookieClickerState() override;

  void update() override;
  void draw(GameGraphics& g) override;

public:
  // Left side
  CookieView* _cookie_view = nullptr;
  Text* _cookie_count_text = nullptr;
  Text* _cps_text = nullptr;

  // Right side
  Button* _tab_upgrades_btn = nullptr;
  Button* _tab_production_btn = nullptr;
  ScrollView* _upgrades_scroll = nullptr;
  ScrollView* _production_scroll = nullptr;

  // Economy
  long double _cookies = 0.0L;
  long double _cookies_per_click = 1.0L;
  long double _cookies_per_second = 0.0L;

  // Example upgrade items
  struct Upgrade {
    std::string name;
    std::string desc;
    long double base_cost;
    long double cost_mult;
    int owned = 0;

    // effect
    long double add_cpc = 0.0L;
    long double add_cps = 0.0L;
  };

  std::vector<Upgrade> _upgrades;
  std::vector<Upgrade> _producers;

  // Timing
  Timer _ui_timer;        // throttle UI work (like BuildState)
  long long _last_ms = 0;
  long double _prod_accum = 0.0L;

  // Input click edge detection
  bool _was_left_down = false;

private:
  void build_ui();
  void rebuild_upgrade_lists();
  void refresh_header_texts();

  void set_tab(bool upgrades);

  bool can_afford(long double cost) const { return _cookies >= cost; }
  void spend(long double cost) { _cookies -= cost; if (_cookies < 0) _cookies = 0; }

  static std::string format_number(long double v);
  static std::string format_rate(long double v);

  CustomLayout* get_component(const Vector2 mouse_pos, CustomLayout* layout, bool only_callback_registered = false);
};

} // namespace Zen
