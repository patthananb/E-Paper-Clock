// Pin map + shared constants for the Waveshare ESP32-S3-ePaper-1.54.
// GPIO assignments verified against Waveshare's ESP32-S3-ePaper-1.54G demo.
#pragma once
#include <Arduino.h>

namespace cm {
namespace Pins {
// e-paper (SSD1681)
constexpr uint8_t EPD_SCK  = 12;
constexpr uint8_t EPD_MOSI = 13;
constexpr uint8_t EPD_CS   = 11;
constexpr uint8_t EPD_DC   = 10;
constexpr uint8_t EPD_RST  = 9;
constexpr uint8_t EPD_BUSY = 8;
constexpr uint8_t EPD_PWR  = 6;    ///< active-low panel power gate (LOW = on)

// buttons
constexpr uint8_t BOOT_BTN = 0;    ///< active-low, internal pull-up
constexpr uint8_t PWR_BTN  = 18;   ///< active-low (assumed; calibrated on first press)

// power latch
constexpr uint8_t VBAT_PWR = 17;   ///< drive HIGH to keep battery power on

// battery sense
constexpr uint8_t BAT_ADC  = 4;    ///< ADC1_CH3, through on-board /2 divider

// I2C (SHTC3 + PCF85063 RTC + ES8311 share the bus)
constexpr uint8_t I2C_SDA  = 47;
constexpr uint8_t I2C_SCL  = 48;
}  // namespace Pins

namespace I2CAddr {
constexpr uint8_t SHTC3 = 0x70;
constexpr uint8_t RTC   = 0x51;
constexpr uint8_t ES8311 = 0x18;
}  // namespace I2CAddr
}  // namespace cm
