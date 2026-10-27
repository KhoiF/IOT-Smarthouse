#pragma once

#include <Arduino.h>

#include "config.h"

// NotClosed: servo đã về vị trí đóng nhưng reed switch vẫn báo hở (cửa kẹt, bị chặn hoặc bị đẩy ra).
enum class DoorState : uint8_t { Closed, Opening, Open, Closing, NotClosed };

// Cửa mô hình: servo mở/đóng theo lệnh (tự đóng sau DOOR_AUTO_CLOSE_MS nếu > 0); giữ mở khi hold (cảnh báo cháy).
class Door {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);

  void open(uint32_t now);
  // Đóng ngay; bỏ qua khi đang giữ mở do cháy.
  void close(uint32_t now);
  void setHold(bool hold, uint32_t now);

  DoorState state() const { return state_; }
  bool held() const { return hold_; }
  // true nếu reed switch báo cửa đóng; luôn true khi chưa lắp cảm biến.
  bool sensorClosed() const;

  static const char* stateName(DoorState state);

 private:
  void moveTo(uint8_t angle, DoorState movingState, uint32_t now);

  DoorState state_ = DoorState::Closed;
  uint32_t stateSince_ = 0;
  uint32_t closeAt_ = 0;
  bool hold_ = false;
  bool pulsing_ = false;
  uint32_t sensorOpenSince_ = 0;  // 0 = reed đang đóng
};
