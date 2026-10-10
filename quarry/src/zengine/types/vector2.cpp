#include "vector2.h"

#include <cmath>
#include <iostream>

namespace Zen {

Vector2::Vector2() {
  _x = 0;
  _y = 0;
}

Vector2::Vector2(const int x, const int y) {
  _x = static_cast<double>(x);
  _y = static_cast<double>(y);
}

Vector2::Vector2(const float x, const float y) {
  _x = static_cast<double>(x);
  _y = static_cast<double>(y);
}

Vector2::Vector2(const double x, const double y) {
  _x = x;
  _y = y;
}

Vector2& Vector2::set_x(const double x) {
  _x = x;
  return *this;
}

Vector2& Vector2::set_y(const double y) {
  _y = y;
  return *this;
}

Vector2& Vector2::add_x(const double x) {
  _x += x;
  return *this;
}

Vector2& Vector2::add_y(const double y) {
  _y += y;
  return *this;
}

double Vector2::get_x() const {
  return _x;
}

double Vector2::get_y() const {
  return _y;
}

int Vector2::get_x_int() const {
  return (int) round(_x);
}

int Vector2::get_y_int() const {
  return (int) round(_y);
}

Vector2& Vector2::add(const Vector2 o) {
  _x += o.get_x();
  _y += o.get_y();
  return *this;
}

Vector2& Vector2::multiply(const Vector2 o) {
  _x *= o.get_x();
  _y *= o.get_y();
  return *this;
}

Vector2& Vector2::scale(const double scalar) {
  _x *= scalar;
  _y *= scalar;
  return *this;
}

double Vector2::get_magnitude() const {
  return sqrt(_x * _x + _y * _y);
}

Vector2& Vector2::normalize() {
  return scale(1 / get_magnitude());
}

Vector2& Vector2::abs() {
  _x = fabs(_x);
  _y = fabs(_y);
  return *this;
}

Vector2& Vector2::negate() {
  _x *= -1;
  _y *= -1;
  return *this;
}

Vector2& Vector2::invert() {
  _x = 1 / _x;
  _y = 1 / _y;
  return *this;
}

Vector2 Vector2::copy() const {
  return {_x, _y};
}

std::ostream& operator<<(std::ostream& os, const Vector2& v2) {
  os << "(" << v2._x << ", " << v2._y << ")";
  return os;
}
}
