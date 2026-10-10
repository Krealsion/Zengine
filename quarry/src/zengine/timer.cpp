#include "../timer.h"

namespace Zen {

std::chrono::time_point<std::chrono::steady_clock> Timer::_current_time = std::chrono::steady_clock::now();
std::chrono::time_point<std::chrono::steady_clock> Timer::_start_time = std::chrono::steady_clock::now();
bool Timer::_automatic_updates = true;

void Timer::update_time() {
  _current_time = std::chrono::steady_clock::now();
}

Timer::Timer(const double delay) {
  _delay = delay;
  _last_real = _current_time;
}

bool Timer::is_time() {
  if (peek_is_time()) {
    _elapsed_time -= _delay;
    return true;
  }
  return false;
}

bool Timer::peek_is_time() const {
  return _elapsed_time >= _delay;
}

double Timer::peek_progress_percentage() const {
  return _elapsed_time / _delay;
}

double Timer::get_time_multiplier() const {
  return _time_multiplier;
}

void Timer::set_time_multiplier(const double time_multiplier) {
  this->_time_multiplier = time_multiplier;
}

bool Timer::is_paused() const {
  return _paused;
}

void Timer::reset() {
  _elapsed_time = 0;
}

void Timer::pause() {
  _paused = true;
}

void Timer::resume() {
  _paused = false;
}

void Timer::_accumulate_time() {
  const auto real_delta = std::chrono::duration<double, std::milli>(_current_time - _last_real).count();
  _last_real = _current_time;

  if (!_paused)
    _elapsed_time += real_delta * _time_multiplier;
}

}