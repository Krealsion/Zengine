#include "cookie_view.h"
#include <algorithm>

namespace Zen {

static int clampi(int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); }

// Small deterministic "random" based on index (no RNG dependency).
static int hash01(int i, int salt) {
  // simple integer hash -> 0..1000
  int x = i * 1103515245 + 12345 + salt * 2654435761;
  x ^= (x >> 16);
  x *= 2246822519u;
  x ^= (x >> 13);
  x *= 3266489917u;
  x ^= (x >> 16);
  return (x & 1023); // 0..1023
}

void CookieView::draw(GameGraphics& g) {
  // Let base draw background/border/children if you want it.
  // For cookie, we usually keep background off and just draw the cookie.
  // If you DO want background, call: CustomLayout::draw(g) first.
  // We'll skip background and just draw cookie + children (none by default):
  for (const auto& child : _children) {
    child->draw(g);
  }

  const auto rect = get_owned_destination();
  const int w = rect.get_width_int();
  const int h = rect.get_height_int();

  if (w <= 2 || h <= 2) return;

  const int cx = rect.get_x_int() + w / 2;
  const int cy = rect.get_y_int() + h / 2;

  const int radius = clampi(std::min(w, h) / 2 - 6, 8, 10000);

  // Cookie base
  g.fill_oval(Rectangle(cx - radius, cy - radius, radius * 2, radius * 2), _cookie_color);

  // Slight darker rim (optional)
  g.draw_oval(Rectangle(cx - radius, cy - radius, radius * 2, radius * 2), Color(
    clampi(_cookie_color.get_red() - 20, 0, 255),
    clampi(_cookie_color.get_green() - 20, 0, 255),
    clampi(_cookie_color.get_blue() - 20, 0, 255),
    _cookie_color.get_alpha()
  ));

  // Chips as little squares scattered inside the circle.
  const int chipSize = clampi(radius / 6, 3, 16);
  const int chipCount = std::max(1, _chip_count);

  for (int i = 0; i < chipCount; i++) {
    // polar-ish placement to keep inside cookie
    const int a = hash01(i, 1);
    const int r = hash01(i, 2);

    const float ang = (a / 1023.0f) * 6.2831853f;
    const float rad = (r / 1023.0f) * (radius - chipSize - 4);

    const int x = static_cast<int>(cx + rad * std::cos(ang));
    const int y = static_cast<int>(cy + rad * std::sin(ang));

    g.fill_rectangle(Rectangle(x - chipSize / 2, y - chipSize / 2, chipSize, chipSize), _chip_color);
  }
}

} // namespace Zen
