#include "rectangle.h"

namespace Zen {

Rectangle::Rectangle(const double x, const double y, const double width, const double height) {
  _position = Vector2(x, y);
  _size = Vector2(width, height);
}

Rectangle::Rectangle(const Vector2 position, const Vector2 size) {
  this->_position = position;
  this->_size = size;
}

Rectangle& Rectangle::set_position(const Vector2 position) {
  this->_position = position;
  return *this;
}

Rectangle& Rectangle::set_size(const Vector2 size) {
  this->_size = size;
  return *this;
}

void Rectangle::set_x(const double x) {
  _position.set_x(x);
}

void Rectangle::set_y(const double y) {
  _position.set_y(y);
}

void Rectangle::set_width(const double width) {
  _size.set_x(width);
}

void Rectangle::set_height(const double height) {
  _size.set_y(height);
}

Vector2 Rectangle::get_position() const {
  return _position;
}

Vector2 Rectangle::get_size() const {
  return _size;
}

double Rectangle::get_x() const {
  return _position.get_x();
}

double Rectangle::get_x_int() const {
  return _position.get_x_int();
}

double Rectangle::get_y() const {
  return _position.get_y();
}

double Rectangle::get_y_int() const {
  return _position.get_y_int();
}

double Rectangle::get_width() const {
  return _size.get_x();
}

double Rectangle::get_width_int() const {
  return _position.get_x_int();
}

double Rectangle::get_height() const {
  return _size.get_y();
}

double Rectangle::get_height_int() const {
  return _position.get_y_int();
}

Rectangle& Rectangle::add(const Rectangle& other) {
  _position.add(other._position);
  _size.add(other._size);
  return *this;
}

bool Rectangle::contains(const Vector2 position) const {
  return position.get_x() >= _position.get_x() &&
    position.get_y() >= _position.get_y() &&
      position.get_x() <= _position.get_x() + _size.get_x() &&
        position.get_y() <= _position.get_y() + _size.get_y();

}

Rectangle Rectangle::copy() const {
  return {_position, _size};
}

Rectangle Rectangle::deep_copy() const {
  return {_position.copy(), _size.copy()};
}
Vector2& Rectangle::get_position_mutable() {
  return _position;
}
Vector2& Rectangle::get_size_mutable() {
  return _size;
}
}
