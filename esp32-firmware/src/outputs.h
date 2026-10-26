#pragma once

#include <Arduino.h>

#include "config.h"

enum class BuzzerPattern : uint8_t { Off, Intrusion, Fire };

// Giữ trạng thái logic của LED/relay/buzzer và ghi ra chân với đúng mức kích.
class Outputs {
 public:
  // Gọi đầu tiên trong setup() để mọi đầu ra ở trạng thái tắt.
  void begin();
  void update(uint32_t now);

  void setLed(uint8_t index, bool on);
  bool led(uint8_t index) const { return leds_[index]; }

  void setRelay(uint8_t index, bool on);
  bool relay(uint8_t index) const { return relays_[index]; }

  void setBuzzerPattern(BuzzerPattern pattern);
  BuzzerPattern buzzerPattern() const { return pattern_; }

 private:
  void writeBuzzer(bool on);

  bool leds_[LED_COUNT] = {};
  bool relays_[RELAY_COUNT] = {};
  BuzzerPattern pattern_ = BuzzerPattern::Off;
  bool buzzerLevelOn_ = false;
  uint32_t buzzerNextToggle_ = 0;
};
