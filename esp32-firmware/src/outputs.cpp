#include "outputs.h"

namespace {

uint8_t pinLevel(bool on, bool activeLow) { return (on != activeLow) ? HIGH : LOW; }

}  // namespace

void Outputs::begin() {
  // Arduino core 3.x bỏ qua digitalWrite() trên chân chưa pinMode(), nên phải
  // pinMode trước rồi ghi mức tắt ngay sau đó.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], pinLevel(false, RELAY_ACTIVE_LOW));
  }
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, pinLevel(false, BUZZER_ACTIVE_LOW));

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
}

void Outputs::setLed(uint8_t index, bool on) {
  if (index >= LED_COUNT) return;
  leds_[index] = on;
  digitalWrite(LED_PINS[index], on ? HIGH : LOW);
}

void Outputs::setRelay(uint8_t index, bool on) {
  if (index >= RELAY_COUNT) return;
  relays_[index] = on;
  digitalWrite(RELAY_PINS[index], pinLevel(on, RELAY_ACTIVE_LOW));
}

void Outputs::setBuzzerPattern(BuzzerPattern pattern) {
  if (pattern == pattern_) return;
  pattern_ = pattern;
  // Bắt đầu mẫu mới bằng một tiếng kêu ngay lập tức.
  writeBuzzer(pattern != BuzzerPattern::Off);
  buzzerNextToggle_ = millis() + (pattern == BuzzerPattern::Fire ? BUZZER_FIRE_ON_MS
                                                                 : BUZZER_INTRUSION_ON_MS);
}

void Outputs::update(uint32_t now) {
  if (pattern_ == BuzzerPattern::Off) return;
  if ((int32_t)(now - buzzerNextToggle_) < 0) return;

  const bool fire = pattern_ == BuzzerPattern::Fire;
  const uint32_t onMs = fire ? BUZZER_FIRE_ON_MS : BUZZER_INTRUSION_ON_MS;
  const uint32_t offMs = fire ? BUZZER_FIRE_OFF_MS : BUZZER_INTRUSION_OFF_MS;

  writeBuzzer(!buzzerLevelOn_);
  buzzerNextToggle_ = now + (buzzerLevelOn_ ? onMs : offMs);
}

void Outputs::writeBuzzer(bool on) {
  buzzerLevelOn_ = on;
  digitalWrite(PIN_BUZZER, pinLevel(on, BUZZER_ACTIVE_LOW));
}
