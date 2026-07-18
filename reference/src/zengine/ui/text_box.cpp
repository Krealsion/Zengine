#include "text_box.h"

#include "input/input.h"
#include "logger.h"
#include "text.h"

namespace Zen {

TextBox::TextBox() : _cursor_blink_timer(500) {
  _cursor_blink_timer.pause();
  CustomLayout::set_background_color(Color(0, 0, 0));
  auto* inner = new CustomLayout();
  inner->set_margin(2, 2, 2, 2); // TODO customizable set_border_width
  inner->set_background_color(Color(255, 255, 255));
  inner->set_size(SizeTo::PARENT, SizeTo::PARENT);
  inner->center();
  inner->set_on_click_callback(std::function([&]() {
    std::cout << "TextBox clicked" << std::endl;
    set_focused(true);
  }));
  CustomLayout::add_child(inner);

  _text = new Text();
  _text->set_size(SizeTo::CHILDREN, SizeTo::CHILDREN);
  _text->set_position(PositionTo::LEFT, PositionTo::CENTER);
  _text->set_wrap(false);
  inner->add_child(_text);
}

TextBox::~TextBox() {
}

void TextBox::update() {
  if (_focused && _cursor_blink_timer.is_time()) {
    _cursor_visible = !_cursor_visible;
    update_text();
  }
  CustomLayout::update();
}


void TextBox::set_focused(const bool focused) {
  _focused = focused;
  if (_focused) {
    _cursor_visible = true;
    // _cursor_blink_timer.reset(); // TODO fix timer and use this to ensure no carryover from past uses
    _cursor_blink_timer.resume();
    Input::start_text_input(Action<const std::string&>([this](const std::string& text) {
      _process_text(text);
    }), Action<>([this]() {
      set_focused(false);
    }));
  } else {
    _cursor_blink_timer.pause();
    Input::end_text_input();
    on_text_committed();
  }
  update_text();
}

void TextBox::_process_text(const std::string& text) {
  if (text == "\0") {
    set_focused(false);
    return;
  }
  if (text == "\b") {
    if (!_text_string.empty()) {
      _text_string.pop_back();
      update_text();
    }
    return;
  }
  if (_filter.type == TextBoxFilterType::ANY) {
    _text_string += text;
  } else if (_filter == TextBoxFilterType::DATA_TYPE && _filter.data_type == DataType::NUMBER ) {
    // Only allow numbers
    for (char c : text) {
      if (c == '-') {
        // Swap sign if the first character is a minus sign
        if (_text_string.empty() || (_text_string.size() == 1 && _text_string[0] == '-')) {
          _text_string = "-" + _text_string;
        } else if (!_text_string.empty() && _text_string[0] == '-') {
          _text_string.erase(0, 1);
        }
      } else if (c == '.') {
        // only allow one decimal point
        if (_text_string.find('.') == std::string::npos) {
          _text_string += c;
        }
      } else if (isdigit(c)) {
        _text_string += c;
      }
    }
  } else if (_filter == TextBoxFilterType::DATA_TYPE && _filter.data_type == DataType::BIT) {
    // Only allow 0 or 1
    for (char c : text) {
      if (c == '0' || c == '1') {
        _text_string += c;
      } else if (c == '-') {
        // Swap sign if the first character is a minus sign
        if (_text_string.empty() || (_text_string.size() == 1 && _text_string[0] == '-')) {
          _text_string = "-" + _text_string;
        } else if (!_text_string.empty() && _text_string[0] == '-') {
          _text_string.erase(0, 1);
        }
      }
    }
  } else if (_filter == TextBoxFilterType::INTEGER) {
    for (char c : text) {
      if (c == '-') {
        // Swap sign if the first character is a minus sign
        if (_text_string.empty() || (_text_string.size() == 1 && _text_string[0] == '-')) {
          _text_string = "-" + _text_string;
        } else if (!_text_string.empty() && _text_string[0] == '-') {
          _text_string.erase(0, 1);
        }
      } else if (isdigit(c)) {
        _text_string += c;
      }
    }
  }
  on_text_changed();
  update_text();
}

void TextBox::set_text(const std::string& text) {
  _text_string = text;
  update_text();
}

std::string TextBox::get_text() const {
  return _text_string;
}

void TextBox::update_text() {
  if (_focused) {
    _text->set_text(_text_string + (_cursor_visible ? "|" : ""));
  } else {
    _text->set_text(_text_string);
  }
}

}
