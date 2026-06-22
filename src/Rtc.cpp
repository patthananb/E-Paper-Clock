#include "Rtc.h"
#include "Pins.h"
#include <Wire.h>

namespace cm {

// PCF85063A register map (auto-increment from any address).
static constexpr uint8_t REG_CTRL1   = 0x00;  ///< STOP / 12_24 / SR
static constexpr uint8_t REG_SECONDS = 0x04;  ///< bit7 = OS (oscillator-stop / clock-integrity)

static inline uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static inline uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

bool RtcClock::begin() {
  // Control_1 = 0x00: oscillator running (STOP=0), 24-hour mode, normal mode.
  // Leaves the time + OS flag untouched, so a previously-set RTC stays valid.
  Wire.beginTransmission(I2CAddr::RTC);
  Wire.write(REG_CTRL1);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;   // false => chip not on the bus
}

bool RtcClock::read(struct tm& out) const {
  Wire.beginTransmission(I2CAddr::RTC);
  Wire.write(REG_SECONDS);
  if (Wire.endTransmission(false) != 0) return false;          // repeated start
  if (Wire.requestFrom((int)I2CAddr::RTC, 7) != 7) return false;

  uint8_t s  = Wire.read();
  uint8_t mi = Wire.read();
  uint8_t h  = Wire.read();
  uint8_t d  = Wire.read();
  uint8_t wd = Wire.read();
  uint8_t mo = Wire.read();
  uint8_t y  = Wire.read();

  if (s & 0x80) return false;            // OS set: oscillator stopped, time not trustworthy

  out.tm_sec   = bcd2dec(s  & 0x7F);
  out.tm_min   = bcd2dec(mi & 0x7F);
  out.tm_hour  = bcd2dec(h  & 0x3F);     // 24h mode -> bits 0..5
  out.tm_mday  = bcd2dec(d  & 0x3F);
  out.tm_wday  = wd & 0x07;
  out.tm_mon   = bcd2dec(mo & 0x1F) - 1; // chip 1..12 -> tm 0..11
  out.tm_year  = bcd2dec(y) + 100;       // chip 00..99 (20xx) -> years since 1900
  out.tm_isdst = 0;

  // Reject obvious garbage (dead backup that still cleared OS, bus glitch).
  if (out.tm_mon < 0 || out.tm_mon > 11 || out.tm_mday < 1 || out.tm_mday > 31 ||
      out.tm_hour > 23 || out.tm_min > 59 || out.tm_sec > 59) return false;
  return true;
}

bool RtcClock::write(const struct tm& in) {
  Wire.beginTransmission(I2CAddr::RTC);
  Wire.write(REG_SECONDS);
  Wire.write(dec2bcd(in.tm_sec) & 0x7F);   // bit7=0 clears the OS flag -> time now valid
  Wire.write(dec2bcd(in.tm_min));
  Wire.write(dec2bcd(in.tm_hour));         // 24h mode
  Wire.write(dec2bcd(in.tm_mday));
  Wire.write(in.tm_wday & 0x07);
  Wire.write(dec2bcd(in.tm_mon + 1));      // tm 0..11 -> chip 1..12
  Wire.write(dec2bcd(in.tm_year % 100));   // years-since-1900 -> 20xx two-digit
  return Wire.endTransmission() == 0;
}

}  // namespace cm
