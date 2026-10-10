#pragma once
#include "custom_layout.h"
#include "timer.h"

#include "zsignal.h"

namespace Zen {
class Text;
class TextBox : public CustomLayout {
public:
  struct TextBoxFilter {
    TextBoxFilterType type = TextBoxFilterType::ANY;
    DataType data_type = DataType::STRING; // Default to string, used when type is DATA_TYPE

    TextBoxFilter(const TextBoxFilterType type, const DataType data_type = DataType::STRING)
        : type(type), data_type(data_type) {}

    TextBoxFilter& operator =(const TextBoxFilterType _type) {
      this->type = _type;
      return *this;
    }
    // Implicit conversion operator to TextBoxFilterType
    operator TextBoxFilterType() const {
      return type;
    }
  };

  Signal on_text_changed;
  Signal on_text_committed;

  TextBox();
  ~TextBox() override;

  // TODO Add a default text

  void update() override;

  void set_focused(bool focused);
  void set_text(const std::string& text);
  std::string get_text() const;

  void set_filter(const TextBoxFilter filter) {
    _filter = filter;
  }
  void set_filter(const TextBoxFilterType filter_type, const DataType data_type = STRING) {
    _filter.type = filter_type;
    _filter.data_type = data_type;
  }

  TextBoxFilter get_filter() const {
    return _filter;
  }

private:
  // TODO add max length
  std::string _text_string;
  Text* _text = nullptr;
  bool _focused = false;
  TextBoxFilter _filter = TextBoxFilter(TextBoxFilterType::ANY);
  Timer _cursor_blink_timer;
  bool _cursor_visible = true;

  void update_text();
  void _process_text(const std::string& text);
};
}
