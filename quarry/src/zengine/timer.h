#pragma once

#include <atomic>
#include <chrono>
#include <iostream>

namespace Zen {

class Timer {
public:
  static void update_time();

  explicit Timer(double delay);

  bool is_time();
  bool peek_is_time() const;

  double peek_progress_percentage() const;

  void set_time_multiplier(double time_multiplier);
  double get_time_multiplier() const;

  bool is_paused() const;

  void reset();
  void pause();
  void resume();

private:
  //The current time as understood by the timer class (Not always up to date)
  static std::chrono::time_point<std::chrono::steady_clock> _current_time;
  static std::chrono::time_point<std::chrono::steady_clock> _last_real;
  static std::chrono::time_point<std::chrono::steady_clock> _start_time;
  static bool _automatic_updates;

  double _elapsed_time = 0.0;
  double _delay;
  double _time_multiplier = 1;
  bool _paused = false;

  void _accumulate_time();
};
}
