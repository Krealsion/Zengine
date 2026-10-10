#pragma once

#include "ui/custom_layout.h"
#include "graphics/game_graphics.h"

namespace Zen {

// A clickable cookie face rendered procedurally: a filled circle + "chips" as tiny squares.
class CookieView : public CustomLayout {
public:
  CookieView() = default;
  ~CookieView() override = default;

  void set_chip_count(int n) { _chip_count = n; }
  void set_cookie_color(const Color& c) { _cookie_color = c; }
  void set_chip_color(const Color& c) { _chip_color = c; }

  // Custom draw: cookie circle + chips.
  void draw(GameGraphics& g) override;

private:
  int _chip_count = 12;
  Color _cookie_color = Color(216, 176, 120);  // cookie-ish tan
  Color _chip_color   = Color(90, 55, 40);     // chocolate-ish brown
};

} // namespace Zen
