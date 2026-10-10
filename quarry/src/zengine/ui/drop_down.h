#pragma once

#include "custom_layout.h"
#include "scroll_view.h"
#include "button.h"
#include "zsignal.h"

#include <vector>
#include <string>

namespace Zen {

class DropDown : public CustomLayout {
public:
  DropDown();
  ~DropDown() override = default;

  void draw(GameGraphics& game_graphics) override;

  // Set the list of options; rebuilds the option panel
  void set_options(const std::vector<std::string>& options);

  void set_on_click_callback(std::function<void()> callback) override;

  // Get/set selected index (-1 for none)
  int get_selected_index() const { return _selected_index; }
  void set_selected_index(int index);

  // Get selected text (empty if none)
  std::string get_selected_text() const;

  // Signal fired when selection changes (passes new index)
  Signal on_selection_changed;

private:
  void _toggle_panel();
  void _hide_panel();
  CustomLayout* _create_option_panel();

  Button* _display_button = nullptr;  // Clickable area for toggle
  CustomLayout* _option_panel = nullptr;
  std::vector<std::string> _options;
  int _selected_index = -1;
  bool _panel_visible = false;
};

}  // namespace Zen
