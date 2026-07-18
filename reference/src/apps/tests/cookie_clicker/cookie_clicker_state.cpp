#include "cookie_clicker_state.h"
#include "../../../src/engine/logger.h"

#include <cmath>
#include <format>
#include <algorithm>

#include "../../../src/engine/timer.h"

namespace Zen {

static long double powld(long double a, long double b) {
  return std::powl(a, b);
}

CookieClickerState::CookieClickerState()
  : _ui_timer(16) // similar cadence as BuildState uses for UI-ish updates :contentReference[oaicite:3]{index=3}
{
  build_ui();

  // Define upgrades (CPC) and producers (CPS)
  _upgrades = {
    {"Better Cursor", "+1 cookies per click", 15.0L, 1.15L, 0, 1.0L, 0.0L},
    {"Reinforced Finger", "+5 cookies per click", 120.0L, 1.17L, 0, 5.0L, 0.0L},
    {"Click Training", "+25 cookies per click", 900.0L, 1.20L, 0, 25.0L, 0.0L},
  };

  _producers = {
    {"Grandma", "+0.1 cookies per second", 10.0L, 1.15L, 0, 0.0L, 0.1L},
    {"Farm", "+1 cookies per second", 110.0L, 1.17L, 0, 0.0L, 1.0L},
    {"Factory", "+8 cookies per second", 1300.0L, 1.20L, 0, 0.0L, 8.0L},
  };

  rebuild_upgrade_lists();
  refresh_header_texts();

  _last_ms = Timer::get_current_time();
}

CookieClickerState::~CookieClickerState() {
  // If you have an ownership system elsewhere, adjust accordingly.
  // For now, follow the pattern used in your codebase (many raw new's).
  // Deleting _root will delete children if your CustomLayout does that; if not, skip.
}

void CookieClickerState::draw(GameGraphics& g) {
}

CustomLayout* CookieClickerState::get_component(const Vector2 mouse_pos, CustomLayout* layout, const bool only_callback_registered) {
  // Same recursive pattern as BuildState :contentReference[oaicite:4]{index=4}
  for (const auto& child : layout->get_children()) {
    if (child->is_visible() && child->get_owned_destination().contains(mouse_pos)) {
      if (auto* found = get_component(mouse_pos, child, only_callback_registered);
          !only_callback_registered || (found->has_callback())) {
        return found;
      }
    }
  }
  return layout;
}

void CookieClickerState::update() {
  // ESC to exit, matching your existing UX expectation
  if (Input::is_key_down(SDL_SCANCODE_ESCAPE)) {
    exit();
    return;
  }

  // Smooth production using dt
  const long long now_ms = Timer::get_current_time();
  const long double dt = std::max(0.0L, (now_ms - _last_ms) / 1000.0L);
  _last_ms = now_ms;

  if (_cookies_per_second > 0.0L) {
    _prod_accum += _cookies_per_second * dt;
    if (_prod_accum >= 0.0001L) {
      _cookies += _prod_accum;
      _prod_accum = 0.0L;
    }
  }

  // Throttle UI text refresh to your timer cadence
  if (_ui_timer.is_time()) {
    refresh_header_texts();
    // could also update enabled/disabled visuals here if you add them later
  }
}

void CookieClickerState::build_ui() {
  _root_layout->set_name("cookie_clicker_root");
  _root_layout->set_horizontal();
  _root_layout->set_background_color(Color(235, 235, 235));
  _root_layout->set_padding(12, 12, 12, 12);
  _root_layout->set_child_spacing(12);

  // LEFT panel
  auto* left = new CustomLayout();
  left->set_name("left_panel");
  left->set_vertical();
  left->set_size(SizeTo::PARENT_PERCENT, 0.62f, SizeTo::PARENT);
  left->set_padding(12, 12, 12, 12);
  left->set_child_spacing(12);
  left->set_background_color(Color(250, 250, 250));

  _cookie_count_text = new Text();
  _cookie_count_text->set_name("cookie_count_text");
  _cookie_count_text->set_text("Cookies: 0");
  left->add_child(_cookie_count_text);

  _cps_text = new Text();
  _cps_text->set_name("cps_text");
  _cps_text->set_text("per second: 0");
  left->add_child(_cps_text);

  // Big cookie button area
  auto* cookie_btn = new Button();
  cookie_btn->set_name("cookie_button");
  cookie_btn->set_text(""); // cookie is visual, no label
  cookie_btn->set_size(SizeTo::PARENT, SizeTo::FILL);
  cookie_btn->set_background_color(Color(245, 245, 245));
  cookie_btn->set_hovered_bg_color(Color(235, 235, 235));
  cookie_btn->set_padding(12, 12, 12, 12);

  // Put cookie view inside the button so clicking the cookie triggers button click.
  _cookie_view = new CookieView();
  _cookie_view->set_name("cookie_view");
  _cookie_view->set_size(SizeTo::STATIC, 360, SizeTo::STATIC, 360);
  _cookie_view->set_position(PositionTo::CENTER, PositionTo::CENTER);
  _cookie_view->set_chip_count(14);

  cookie_btn->add_child(_cookie_view);

  cookie_btn->set_on_click_callback(Action<>([this]() {
    _cookies += _cookies_per_click;
    refresh_header_texts();
  }));

  left->add_child(cookie_btn);

  // RIGHT panel
  auto* right = new CustomLayout();
  right->set_name("right_panel");
  right->set_vertical();
  right->set_size(SizeTo::FILL, SizeTo::PARENT);
  right->set_padding(12, 12, 12, 12);
  right->set_child_spacing(10);
  right->set_background_color(Color(250, 250, 250));

  // Tabs row
  auto* tabs = new CustomLayout();
  tabs->set_name("tabs_row");
  tabs->set_horizontal();
  tabs->set_size(SizeTo::PARENT, SizeTo::CHILDREN);
  tabs->set_child_spacing(8);

  _tab_upgrades_btn = new Button();
  _tab_upgrades_btn->set_name("tab_upgrades");
  _tab_upgrades_btn->set_text("Upgrades");
  _tab_upgrades_btn->set_size(SizeTo::FILL, SizeTo::CHILDREN);
  _tab_upgrades_btn->set_background_color(Color(200, 200, 200));
  _tab_upgrades_btn->set_hovered_bg_color(Color(70, 120, 200));
  _tab_upgrades_btn->set_on_click_callback(Action<>([this]() { set_tab(true); }));

  _tab_production_btn = new Button();
  _tab_production_btn->set_name("tab_production");
  _tab_production_btn->set_text("Production");
  _tab_production_btn->set_size(SizeTo::FILL, SizeTo::CHILDREN);
  _tab_production_btn->set_background_color(Color(200, 200, 200));
  _tab_production_btn->set_hovered_bg_color(Color(70, 120, 200));
  _tab_production_btn->set_on_click_callback(Action<>([this]() { set_tab(false); }));

  tabs->add_child(_tab_upgrades_btn);
  tabs->add_child(_tab_production_btn);
  right->add_child(tabs);

  _upgrades_scroll = new ScrollView();
  _upgrades_scroll->set_name("upgrades_scroll");
  _upgrades_scroll->set_vertical();
  _upgrades_scroll->set_size(SizeTo::PARENT, SizeTo::FILL);
  _upgrades_scroll->set_background_color(Color(245, 245, 245));

  _production_scroll = new ScrollView();
  _production_scroll->set_name("production_scroll");
  _production_scroll->set_vertical();
  _production_scroll->set_size(SizeTo::PARENT, SizeTo::FILL);
  _production_scroll->set_background_color(Color(245, 245, 245));

  right->add_child(_upgrades_scroll);
  right->add_child(_production_scroll);

  _root_layout->add_child(left);
  _root_layout->add_child(right);

  set_tab(true);
}

void CookieClickerState::set_tab(bool upgrades) {
  if (_upgrades_scroll) _upgrades_scroll->set_visible(upgrades);
  if (_production_scroll) _production_scroll->set_visible(!upgrades);

  // Visual hint which tab is active
  if (_tab_upgrades_btn) {
    _tab_upgrades_btn->set_background_color(upgrades ? Color(70,120,200) : Color(200,200,200));
  }
  if (_tab_production_btn) {
    _tab_production_btn->set_background_color(!upgrades ? Color(70,120,200) : Color(200,200,200));
  }
}

static long double current_cost(const CookieClickerState::Upgrade& u) {
  // Exponential scaling => "infinite" progression
  return u.base_cost * powld(u.cost_mult, (long double)u.owned);
}

void CookieClickerState::rebuild_upgrade_lists() {
  if (_upgrades_scroll) _upgrades_scroll->remove_all_children();
  if (_production_scroll) _production_scroll->remove_all_children();

  auto make_row = [&](Upgrade& u, bool is_producer) -> CustomLayout* {
    auto* row = new CustomLayout();
    row->set_horizontal();
    row->set_size(SizeTo::PARENT, SizeTo::CHILDREN);
    row->set_padding(8, 8, 8, 8);
    row->set_child_spacing(10);

    auto* info = new CustomLayout();
    info->set_vertical();
    info->set_size(SizeTo::PARENT_PERCENT, 0.62f, SizeTo::CHILDREN);
    info->set_child_spacing(2);

    auto* title = new Text();
    title->set_size(SizeTo::CHILDREN, SizeTo::CHILDREN);
    title->set_text(u.name + "  (owned: " + std::to_string(u.owned) + ")");
    info->add_child(title);

    auto* desc = new Text();
    desc->set_size(SizeTo::CHILDREN, SizeTo::CHILDREN);
    desc->set_text(u.desc);
    desc->set_wrap(true);
    info->add_child(desc);

    auto* buy = new Button();
    buy->set_text("Buy");
    buy->set_size(SizeTo::FILL, SizeTo::CHILDREN);
    buy->set_background_color(Color(200, 200, 200));
    buy->set_hovered_bg_color(Color(70, 120, 200));

    auto* cost_txt = new Text();
    cost_txt->set_text("Cost: " + format_number(current_cost(u)));
    cost_txt->set_size(SizeTo::CHILDREN, SizeTo::CHILDREN);

    buy->set_on_click_callback(Action<>([this, &u, cost_txt]() {
      const long double cost = current_cost(u);
      if (!can_afford(cost)) return;

      spend(cost);
      u.owned += 1;
      _cookies_per_click += u.add_cpc;
      _cookies_per_second += u.add_cps;

      cost_txt->set_text("Cost: " + format_number(current_cost(u)));
      rebuild_upgrade_lists();
      refresh_header_texts();
    }));

    row->add_child(info);

    auto* rightCol = new CustomLayout();
    rightCol->set_vertical();
    rightCol->set_size(SizeTo::FILL, SizeTo::CHILDREN);
    rightCol->set_child_spacing(4);

    rightCol->add_child(cost_txt);
    rightCol->add_child(buy);

    row->add_child(rightCol);

    // soft row background
    row->set_background_color(Color(255,255,255));
    return row;
  };

  // Upgrades
  if (_upgrades_scroll) {
    auto* container = new CustomLayout();
    container->set_vertical();
    container->set_size(SizeTo::PARENT, SizeTo::CHILDREN);
    container->set_child_spacing(8);
    container->set_padding(8,8,8,8);

    for (auto& u : _upgrades) container->add_child(make_row(u, false));
    _upgrades_scroll->add_child(container);
  }

  // Producers
  if (_production_scroll) {
    auto* container = new CustomLayout();
    container->set_vertical();
    container->set_size(SizeTo::PARENT, SizeTo::CHILDREN);
    container->set_child_spacing(8);
    container->set_padding(8,8,8,8);

    for (auto& u : _producers) container->add_child(make_row(u, true));
    _production_scroll->add_child(container);
  }
}

void CookieClickerState::refresh_header_texts() {
  if (_cookie_count_text) {
    _cookie_count_text->set_text("Cookies: " + format_number(_cookies));
  }
  if (_cps_text) {
    _cps_text->set_text("per second: " + format_rate(_cookies_per_second) +
                        "   |   per click: " + format_number(_cookies_per_click));
  }
}

std::string CookieClickerState::format_rate(long double v) {
  return format_number(v) + "/s";
}

std::string CookieClickerState::format_number(long double v) {
  // Abbreviate: K, M, B, T, Qa, Qi...
  // Keep it simple + stable (good enough to validate engine flow).
  if (v < 0) v = 0;

  static const char* suffix[] = {"", "K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc", "No"};
  int s = 0;

  while (v >= 1000.0L && s < (int)(sizeof(suffix)/sizeof(suffix[0])) - 1) {
    v /= 1000.0L;
    s++;
  }

  // show 0 decimals for small, 2 decimals for large
  if (s == 0) {
    // integer-ish for small values
    return std::format("{}", (long long)std::llround(v));
  }

  // 2 decimals max
  return std::format("{:.2f}{}", (double)v, suffix[s]);
}

} // namespace Zen
