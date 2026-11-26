#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "door.h"
#include "outputs.h"
#include "sensors.h"

enum class CommandResult : uint8_t { Ok, InvalidTarget, BlockedByFire, DoorHeldByFire };

// Logic chạy cục bộ trên ESP32, không phụ thuộc mạng:
// - Cháy (MQ-2 hoặc nhiệt độ): còi kiểu Fire, ngắt điều hòa (relay 1), bật quạt thông gió (relay 2).
// - An ninh BẬT: PIR phát hiện chuyển động -> báo trộm, hết chuyển động thì còi tự tắt.
// - Tối + chuyển động (An ninh bật hay tắt) -> bật LED phòng khách, hết chuyển động thì tắt; tắt tay thì
//   chặn tự bật lại 30 giây; đèn bật bằng
//   lệnh tay thì không tự tắt.
// - Trời tối -> bật LED ban công.
// - Nhiệt độ > AC_ON_C -> bật điều hòa (relay 1), không tự tắt.
// - Khí gas mức cao hoặc báo cháy -> bật quạt thông gió (relay 2), hết thì tắt.
// - Cửa: mở theo lệnh (kèm tắt An ninh), đóng theo lệnh; cháy -> mở để thoát hiểm (giữ An ninh);
//   cửa bị mở trái phép khi An ninh bật -> báo trộm.
// - Lệnh tay/AI trên đầu ra tự động giữ ưu tiên USER_OVERRIDE_MS hoặc tới khi cảm biến đổi trạng thái.
class Controller {
 public:
  Controller(Sensors& sensors, Outputs& outputs, Door& door)
      : sensors_(sensors), outputs_(outputs), door_(door) {}

  void begin(uint32_t now);
  void update(uint32_t now);

  CommandResult setLedByUser(uint8_t index, bool on, uint32_t now);
  CommandResult setRelayByUser(uint8_t index, bool on, uint32_t now);
  CommandResult openDoorByUser(uint32_t now);
  CommandResult closeDoorByUser(uint32_t now);
  void setArmed(bool armed, uint32_t now);
  void silenceAlarm();
  void simulateGas(bool active);

  bool armed() const { return armed_; }
  bool armingPending(uint32_t now) const;
  bool fireAlarm() const { return gasAlarm_ || heatAlarm_; }
  bool gasAlarm() const { return gasAlarm_; }
  bool heatAlarm() const { return heatAlarm_; }
  bool intrusionAlarm() const { return intrusionAlarm_; }
  bool doorForced() const { return doorForced_; }
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
  void applyLivingLight(uint32_t now);
  static void setOverride(Override& o, bool value, uint32_t now);
  static bool isAutomatedLed(uint8_t index) {
    return index == LED_BAN_CONG;
  }
  void event(const char* message);

  Sensors& sensors_;
  Outputs& outputs_;
  Door& door_;
  Preferences prefs_;

  bool armed_ = false;
  uint32_t armActiveAt_ = 0;

  bool mq2Alarm_ = false;  // chỉ từ cảm biến, không gồm giả lập
  bool mq2Above_ = false;
  uint32_t mq2AboveSince_ = 0;
  bool gasAlarm_ = false;
  bool heatAlarm_ = false;
  bool simulatedGas_ = false;
  bool fireSilenced_ = false;
  bool intrusionAlarm_ = false;
  bool intrusionSilenced_ = false;
  uint32_t lastIntrusionMotionAt_ = 0;
  bool doorForced_ = false;
  DoorState lastDoorState_ = DoorState::Closed;

  bool livingAutoOn_ = false;     // điều kiện tự động: tối + chuyển động, chưa hết thời gian chờ
  bool livingLightAuto_ = false;  // LED phòng khách đang sáng do tự động hóa bật
  bool livingManualOff_ = false;  // vừa tắt tay: chặn tự bật tới livingManualOffUntil_ hoặc hết chuyển động
  uint32_t livingManualOffUntil_ = 0;
  uint32_t lastMotionAt_ = 0;
  bool acHot_ = false;     // đang nóng (> AC_ON_C), chờ xuống AC_REARM_C để xét lần nóng kế tiếp
  bool acPending_ = false; // vừa nóng nhưng chưa bật được (đang cháy)
  bool fanGas_ = false;    // MQ-2 ở mức cao (có hysteresis)

  Override ledOverrides_[LED_COUNT];
  Override relayOverrides_[RELAY_COUNT];
  bool ledLastAuto_[LED_COUNT] = {};
  bool relayLastAuto_[RELAY_COUNT] = {};

  bool changed_ = true;
};
