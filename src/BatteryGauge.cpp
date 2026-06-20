#include "BatteryGauge.h"
#include "Pins.h"

namespace cm {

static constexpr float BAT_DIVIDER = 2.0f;   // on-board /2 resistor divider
static constexpr int   BAT_SAMPLES = 16;

void BatteryGauge::begin() {
  analogReadResolution(12);
  analogRead(Pins::BAT_ADC);                            // configure as analog first
  analogSetPinAttenuation(Pins::BAT_ADC, ADC_11db);     // full ~0-3.3V range
  _pct = readPercent();
}

float BatteryGauge::readVolts() {
  uint32_t mv = 0;
  for (int i = 0; i < BAT_SAMPLES; i++) mv += analogReadMilliVolts(Pins::BAT_ADC);
  mv /= BAT_SAMPLES;
  _volts = (mv / 1000.0f) * BAT_DIVIDER;
  return _volts;
}

// Single-cell Li-po discharge curve (open-circuit-ish). Volts -> percent.
int BatteryGauge::toPercent(float v) {
  static const float pts[][2] = {
    {4.20f,100},{4.10f,90},{4.00f,80},{3.90f,70},{3.80f,60},
    {3.70f,50},{3.60f,35},{3.50f,20},{3.40f,12},{3.30f,6},
    {3.20f,3},{3.00f,0}};
  const int n = sizeof(pts) / sizeof(pts[0]);
  if (v >= pts[0][0])   return 100;
  if (v <= pts[n-1][0]) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (v <= pts[i][0] && v > pts[i+1][0]) {
      float t = (v - pts[i+1][0]) / (pts[i][0] - pts[i+1][0]);
      return (int)roundf(pts[i+1][1] + t * (pts[i][1] - pts[i+1][1]));
    }
  }
  return 0;
}

int BatteryGauge::readPercent() {
  _pct = toPercent(readVolts());
  return _pct;
}

}  // namespace cm
