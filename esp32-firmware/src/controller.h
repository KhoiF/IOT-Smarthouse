#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "outputs.h"
#include "sensors.h"

enum class CommandResult : uint8_t { Ok, InvalidTarget, BlockedByFire };

// Logic chạy cục bộ trên ESP32, không phụ thuộc mạng:
// - Cháy (MQ-2 hoặc nhiệt độ): còi kiểu Fire, ngắt cả hai relay.
// - An ninh BẬT: PIR phát hiện chuyển động -> báo trộm, hết chuyển động thì còi tự tắt.
// - An ninh TẮT: tối + chuyển động -> bật LED phòng khách, hết chuyển động thì tắt.
// - Trời tối -> bật LED ban công. Nhiệt độ cao -> bật quạt (relay 1).
// - Lệnh tay/AI trên đầu ra tự động giữ ưu tiên USER_OVERRIDE_MS hoặc tới khi cảm biến đổi trạng thái.
class Controller {
 public:
  Controller(Sensors& sensors, Outputs& outputs) : sensors_(sensors), outputs_(outputs) {}

  void begin(uint32_t now);
  void update(uint32_t now);

  CommandResult setLedByUser(uint8_t index, bool on, uint32_t now);
  CommandResult setRelayByUser(uint8_t index, bool on, uint32_t now);
  void setArmed(bool armed, uint32_t now);
  void silenceAlarm();
  void simulateGas(bool active);

  bool armed() const { return armed_; }
  bool armingPending(uint32_t now) const;
  bool fireAlarm() const { return gasAlarm_ || heatAlarm_; }
  bool gasAlarm() const { return gasAlarm_; }
  bool heatAlarm() const { return heatAlarm_; }
  bool intrusionAlarm() const { return intrusionAlarm_; }
  bool alarmSilenced() const;
  bool gasSimulated() const { return simulatedGas_; }
  bool overrideActive(bool isRelay, uint8_t index, uint32_t now) const;

  // true nếu có đầu ra hoặc cảnh báo đổi kể từ lần gọi trước (để gửi telemetry ngay).
  bool consumeChanged();

 private:
  struct Override {
    bool active = false;
    bool value = false;
    uint32_t until = 0;
  };

  void updateAlarms(uint32_t now);
  void updateAutomation(uint32_t now);
  void applyBuzzer();
  void applyAutomated(bool isRelay, uint8_t index, bool autoValue, uint32_t now);
  static void setOverride(Override& o, bool value, uint32_t now);
  static bool isAutomatedLed(uint8_t index) {
    return index == LED_PHONG_KHACH || index == LED_BAN_CONG;
  }
  void event(const char* message);

  Sensors& sensors_;
  Outputs& outputs_;
  Preferences prefs_;

  bool armed_ = false;
  uint32_t armActiveAt_ = 0;

  bool mq2Alarm_ = false;  // chỉ từ cảm biến, không gồm giả lập
  bool gasAlarm_ = false;
  bool heatAlarm_ = false;
  bool simulatedGas_ = false;
  bool fireSilenced_ = false;
  bool intrusionAlarm_ = false;
  bool intrusionSilenced_ = false;
  uint32_t lastIntrusionMotionAt_ = 0;

  bool livingAutoOn_ = false;
  uint32_t lastMotionAt_ = 0;
  bool fanAutoOn_ = false;

  Override ledOverrides_[LED_COUNT];
  Override relayOverrides_[RELAY_COUNT];
  bool ledLastAuto_[LED_COUNT] = {};
  bool relayLastAuto_[RELAY_COUNT] = {};

  bool changed_ = true;
};
