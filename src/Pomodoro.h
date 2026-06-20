// Pomodoro timer. start() begins a 25-min countdown; stop() cancels; finished
// latches once when the countdown elapses (consumed by the caller to beep).
#pragma once
#include <Arduino.h>

namespace cm {

class Pomodoro {
public:
  void start(uint32_t minutes = 25);
  void stop();
  bool isActive() const { return _active; }

  // Remaining time, clamped at 0.
  uint32_t remainingMs() const;
  int      remainingMin() const { return remainingMs() / 60000; }
  int      remainingSec() const { return (remainingMs() % 60000) / 1000; }

  // Remaining fraction 0..1 (for the countdown ring).
  float    fraction() const;

  // True exactly once when the timer reaches 0 (edge). Call each loop.
  bool tickFinished();

private:
  bool     _active   = false;
  uint32_t _endMs    = 0;
  uint32_t _totalMs  = 0;
};

}  // namespace cm
