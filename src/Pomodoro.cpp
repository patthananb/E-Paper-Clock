#include "Pomodoro.h"

namespace cm {

void Pomodoro::start(uint32_t minutes) {
  _active  = true;
  _totalMs = minutes * 60UL * 1000UL;
  _endMs   = millis() + _totalMs;
}

float Pomodoro::fraction() const {
  if (!_active || _totalMs == 0) return 0.0f;
  return (float)remainingMs() / (float)_totalMs;
}

void Pomodoro::stop() {
  _active = false;
}

uint32_t Pomodoro::remainingMs() const {
  if (!_active) return 0;
  int32_t rem = (int32_t)(_endMs - millis());
  return rem > 0 ? (uint32_t)rem : 0;
}

bool Pomodoro::tickFinished() {
  if (!_active) return false;
  if ((int32_t)(_endMs - millis()) <= 0) {
    _active = false;
    return true;            // edge: fired once
  }
  return false;
}

}  // namespace cm
