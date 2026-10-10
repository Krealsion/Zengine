#include "drop_down.h"
#include "input/input.h"
#include "state_management/game_state.h"
#include "state_management/game_state_manager.h"

namespace Zen {

DropDown::DropDown() {
  set_vertical();
  _display_button = new Button();
  _display_button->set_name("Dropdown Display Button");
  _display_button->set_border(0);
  _display_button->set_border_color(Color(130, 130, 200));
  _display_button->set_size(SizeTo::PARENT, SizeTo::PARENT);
  _display_button->set_background_color(Color(200, 200, 200));
  _display_button->set_hovered_bg_color(Color(220, 220, 220));
  _display_button->set_text_position(PositionTo::LEFT, PositionTo::CENTER);
  _display_button->set_on_click_callback([this]() {
    _toggle_panel();
  });
  CustomLayout::add_child(_display_button);
}

void DropDown::set_on_click_callback(std::function<void()> callback) {
  CustomLayout::set_on_click_callback(Action([this, callback]() {
    callback(); // TODO WHICH FIRST? DOES IT MATTER? AHHHHHH!
    _toggle_panel();
  }));
}

void DropDown::draw(GameGraphics& game_graphics) {
  CustomLayout::draw(game_graphics);
}

void DropDown::set_options(const std::vector<std::string>& options) {
  _options = options;
  set_selected_index(0);
}

void DropDown::set_selected_index(const int index) {
  if (index >= 0 && index < _options.size()) {
    _selected_index = index;
    _display_button->set_text(_options[index]);
    on_selection_changed();
    _hide_panel();
  }
}

std::string DropDown::get_selected_text() const {
  if (_selected_index >= 0 && _selected_index < _options.size()) {
    return _options[_selected_index];
  }
  return "";
}

void DropDown::_toggle_panel() {
  if (_option_panel) {
    delete _option_panel;
  } else {
    _option_panel = _create_option_panel();
    GameStateManager::singleton().get_current_state()->add_overlay(_option_panel);
    GameStateManager::singleton().get_current_state()->add_mouse_callback([this](const Vector2 mouse_pos) -> bool {
      if (!_option_panel) { return false;}
      if (_option_panel->get_owned_destination().contains(mouse_pos)) {
        _option_panel->click_location(mouse_pos);
        return true;
      }
      _hide_panel();
      return false;
    }, _option_panel);
  }
}

void DropDown::_hide_panel() {
  GameStateManager::singleton().get_current_state()->remove_overlay(_option_panel);
  delete _option_panel;
  _option_panel = nullptr;
}

CustomLayout* DropDown::_create_option_panel() {
  // Ephemeral popup
  auto option_panel = new ScrollView();
  option_panel->set_name("option_panel");
  option_panel->set_parent(this);
  // TODO FINISH UPGRADING DROPDOWN CURRENTLY USUSABLE
  // Either popup it using ABSOLUTE position, or relative it with an input registration so input knows to look for it
  option_panel->set_position(PositionTo::RELATIVE, 0,
                            PositionTo::RELATIVE, static_cast<int>(std::round(this->get_height()))); // TODO use const updating var for pos
  option_panel->set_vertical();
  option_panel->set_size(SizeTo::PARENT, 0, SizeTo::STATIC, 105);
  option_panel->set_background_color(Color(255, 255, 255));
  for (size_t i = 0; i < _options.size(); i++) {
    auto* option_button = new Button();
    option_button->set_text(_options[i]);
    option_button->set_size(SizeTo::PARENT, SizeTo::CHILDREN);
    option_button->set_background_color(Color(220, 220, 220));
    option_button->set_auto_hover_color(true);
    option_button->set_border(0);

    // Capture index for callback
    int idx = static_cast<int>(i);
    option_button->set_on_click_callback([this, idx]() {
      set_selected_index(idx);
    });
    option_panel->add_child(option_button);
    if (i != _options.size() - 1) {
      auto separator = new CustomLayout();
      separator->center();
      separator->set_size(SizeTo::PARENT_PERCENT, .9, SizeTo::STATIC, 1); // TODO configs
      separator->set_background_color(Color(20, 20, 80)); // TODO configs
      option_panel->add_child(separator);
    }
  }
  return option_panel;
}
}  // namespace Zen
