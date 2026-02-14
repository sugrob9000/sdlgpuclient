#pragma once
#include <chrono>
#include <thread>

class frame_pacer {
  using clock = std::chrono::steady_clock;
  clock::time_point next_wake = clock::now();
  clock::duration interval;

public:
  explicit frame_pacer(int fps):
    interval(std::chrono::nanoseconds(int(1e9)) / fps) {}

  void wait_next() {
    next_wake += interval;
    std::this_thread::sleep_until(next_wake);
  }
};