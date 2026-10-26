#pragma once

#include <Arduino.h>

#include "config.h"

enum class DoorState : uint8_t { Closed, Opening, Open, Closing };

// Cửa mô hình: servo mở/đóng, tự đóng sau DOOR_OPEN_MS; giữ mở khi hold (cảnh báo cháy).
class Door {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);

  void open(uint32_t now);
  void setHold(bool hold, uint32_t now);

  DoorState state() const { return state_; }
  bool held() const { return hold_; }
  // true nếu reed switch báo cửa đóng; luôn true khi chưa lắp cảm biến.
  bool sensorClosed() const;
  // Cửa đã ở trạng thái Closed bao lâu (ms).
  uint32_t closedFor(uint32_t now) const { return state_ == DoorState::Closed ? now - stateSince_ : 0; }

  static const char* stateName(DoorState state);

 private:
  void moveTo(uint8_t angle, DoorState movingState, uint32_t now);

  DoorState state_ = DoorState::Closed;
  uint32_t stateSince_ = 0;
  uint32_t closeAt_ = 0;
  bool hold_ = false;
  bool pulsing_ = false;
};
