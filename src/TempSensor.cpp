#include "TempSensor.h"
#include "Pins.h"
#include <Wire.h>

namespace cm {

bool TempSensor::read() {
  Wire.beginTransmission(I2CAddr::SHTC3);          // wake-up
  Wire.write(0x35); Wire.write(0x17);
  if (Wire.endTransmission() != 0) return false;
  delayMicroseconds(300);

  Wire.beginTransmission(I2CAddr::SHTC3);          // measure T first, no clock-stretch
  Wire.write(0x78); Wire.write(0x66);
  if (Wire.endTransmission() != 0) return false;
  delay(15);                                       // ~12ms conversion

  if (Wire.requestFrom((int)I2CAddr::SHTC3, 6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];   // b[2] = T CRC (skipped)
  _tC = -45.0f + 175.0f * (float)rawT / 65535.0f;
  uint16_t rawRH = ((uint16_t)b[3] << 8) | b[4];  // b[5] = RH CRC (skipped)
  _rh = 100.0f * (float)rawRH / 65535.0f;

  Wire.beginTransmission(I2CAddr::SHTC3);          // sleep
  Wire.write(0xB0); Wire.write(0x98);
  Wire.endTransmission();
  return true;
}

}  // namespace cm
