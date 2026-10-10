#include "text.h"

// #include "generated/config.h"
#include "../../config_manager.h"
#include "graphics/game_graphics.h"
#include "graphics/texture_manager.h"
#include "message_bus/var_storage.h"

namespace Zen {

void BaseText::draw(GameGraphics& game_graphics) {
  CustomLayout::draw(game_graphics);
  _update_text();
  // TODO add this as a debugging feature
  // auto dest = get_owned_destination();
  // game_graphics.draw_rectangle(dest, Color(255, 0, 255, 128));
  // auto p_dest = _parent->get_owned_destination();
  // game_graphics.draw_rectangle(p_dest, Color(0, 255, 255, 128));
  if (_wrap) {
    if (get_width() < _max_width) {
      game_graphics.draw_text(_text, _font, _font_size.get_value(), _font_color, get_size().get_x_int(), get_position());
    } else {
      game_graphics.draw_text(_text, _font, _font_size.get_value(), _font_color, _max_width, get_position());
    }
  } else {
    game_graphics.draw_text(_text, _font, _font_size.get_value(), _font_color, 0, get_position());
  }
}

void BaseText::_update_text() {
  if (!_font.empty() && _font_size.get_value() > 0) {
    const Vector2 text_size = TextureManager::get_text_size(_text, _font, _font_size.get_value(), _max_width);
    CustomLayout::set_width(SizeTo::STATIC, text_size.get_x() + _padding_value_right + _padding_value_left);
    CustomLayout::set_height(SizeTo::STATIC, text_size.get_y() + _padding_value_top + _padding_value_bottom);
  }
}

Text::Text() {
  _create_base_text();
}

Text::Text(const Config& config) {
  _create_base_text();
  this->CustomLayout::set_size(SizeTo::CHILDREN, SizeTo::CHILDREN);
  this->set_font(config.font_name);
  this->set_font_size(config.font_size);
  this->set_font_color(config.font_color);
}

void Text::_create_base_text() {
  _base_text = new BaseText();
  set_font("Basic-Regular.ttf");
  // set_font_size(Zen::Config::ui::font_size());
  // _base_text->_font_size.setf(&Zen::Config::ui::font_size);
  _base_text->_update_text();
  set_font_color(Color(0, 0, 0));
  _base_text->set_position(Zen::PositionTo::CENTER, Zen::PositionTo::CENTER);
  add_child(_base_text);
}

void Text::set_wrap(const bool should_wrap) {
  _base_text->_wrap = should_wrap;
  if (!_base_text->_wrap) {
    _max_width = 0;
  }
}

void Text::set_text(const std::string& text) const {
  _base_text->_text = text;
  _base_text->_update_text();
}

void Text::set_font(const std::string& font) const {
  _base_text->_font = font;
  _base_text->_update_text();
}

void Text::set_font_size(const float font_size) const {
  _base_text->_font_size = font_size;
  _base_text->_update_text();
}

void Text::set_font_color(const Color font_color) const {
  _base_text->_font_color = font_color;
}
const std::string& Text::get_text() const {
  return _base_text->_text;
}
const std::string& Text::get_font() const {
  return _base_text->_font;
}
float Text::get_font_size() const {
  return _base_text->_font_size.get_value();
}
Color Text::get_font_color() const {
  return _base_text->_font_color;
}
bool Text::get_wrap() const {
  return _base_text->_wrap;
}
} // namespace Zen