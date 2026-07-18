#pragma once
#include <cstdint>
#include <string>
namespace Zen::Config {
namespace graphics {
  double get_ui_scale();
  bool get_vsync();
}
namespace ui {
  int64_t get_base_padding();
  bool get_enable_animations();
  double get_font_size();
  namespace button {
    std::string get_default_text();
  }
}
}
