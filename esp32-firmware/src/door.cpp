#include "door.h"

namespace {

bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

constexpr uint32_t SERVO_FREQ_HZ = 50;
constexpr uint8_t SERVO_RESOLUTION_BITS = 14;
constexpr uint32_t SERVO_PERIOD_US = 1000000 / SERVO_FREQ_HZ;

uint32_t angleToDuty(uint8_t angle) {
  const uint32_t pulseUs = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle / 180;
  return pulseUs * ((1UL << SERVO_RESOLUTION_BITS) - 1) / SERVO_PERIOD_US;
}

}  // namespace

void Door::begin(uint32_t now) {
  if (DOOR_SENSOR_ENABLED) pinMode(PIN_DOOR_SENSOR, INPUT_PULLUP);
  ledcAttach(PIN_DOOR_SERVO, SERVO_FREQ_HZ, SERVO_RESOLUTION_BITS);
  // Đưa servo về vị trí đóng lúc khởi động.
  moveTo(DOOR_CLOSED_ANGLE, DoorState::Closing, now);
}

void Door::update(uint32_t now) {
  const bool moved = now - stateSince_ >= DOOR_MOVE_MS;

  // Ngắt xung khi servo đã tới vị trí để không rung và đỡ tốn điện.
  if (pulsing_ && moved && (state_ == DoorState::Opening || state_ == DoorState::Closing)) {
    ledcWrite(PIN_DOOR_SERVO, 0);
    pulsing_ = false;
  }

  if (state_ == DoorState::Opening && moved) {
    state_ = DoorState::Open;
    stateSince_ = now;
  } else if (state_ == DoorState::Closing && moved) {
    state_ = DoorState::Closed;
    stateSince_ = now;
  } else if (state_ == DoorState::Open && !hold_ && DOOR_AUTO_CLOSE_MS > 0 && timeReached(now, closeAt_)) {
    moveTo(DOOR_CLOSED_ANGLE, DoorState::Closing, now);
  }

  // Đối chiếu với reed switch khi servo đã ở vị trí đóng.
  if (state_ != DoorState::Closed && state_ != DoorState::NotClosed) {
    sensorOpenSince_ = 0;
  } else if (sensorClosed()) {
    sensorOpenSince_ = 0;
    if (state_ == DoorState::NotClosed) {
      state_ = DoorState::Closed;
      stateSince_ = now;
    }
  } else {
    if (sensorOpenSince_ == 0) sensorOpenSince_ = now;
    if (state_ == DoorState::Closed && now - sensorOpenSince_ >= DOOR_NOT_CLOSED_MS) {
      state_ = DoorState::NotClosed;
      stateSince_ = now;
    }
  }
}

void Door::open(uint32_t now) {
  closeAt_ = now + DOOR_AUTO_CLOSE_MS;
  if (state_ == DoorState::Closed || state_ == DoorState::Closing || state_ == DoorState::NotClosed) {
    moveTo(DOOR_OPEN_ANGLE, DoorState::Opening, now);
  }
}

void Door::close(uint32_t now) {
  if (hold_) return;
  if (state_ == DoorState::Open || state_ == DoorState::Opening) {
    moveTo(DOOR_CLOSED_ANGLE, DoorState::Closing, now);
  }
}

void Door::setHold(bool hold, uint32_t now) {
  if (hold == hold_) return;
  hold_ = hold;
  if (hold) {
    open(now);
  } else {
    closeAt_ = now + DOOR_AUTO_CLOSE_MS;
  }
}

bool Door::sensorClosed() const {
  return !DOOR_SENSOR_ENABLED || digitalRead(PIN_DOOR_SENSOR) == LOW;
}

const char* Door::stateName(DoorState state) {
  switch (state) {
    case DoorState::Closed:
      return "closed";
    case DoorState::Opening:
      return "opening";
    case DoorState::Open:
      return "open";
    case DoorState::Closing:
      return "closing";
    case DoorState::NotClosed:
      return "not_closed";
  }
  return "closed";
}

void Door::moveTo(uint8_t angle, DoorState movingState, uint32_t now) {
  ledcWrite(PIN_DOOR_SERVO, angleToDuty(angle));
  pulsing_ = true;
  state_ = movingState;
  stateSince_ = now;
}
