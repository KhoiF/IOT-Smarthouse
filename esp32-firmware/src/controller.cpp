#include "controller.h"

namespace {

bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

constexpr const char* PREFS_NAMESPACE = "smarthome";
constexpr const char* PREFS_ARMED = "armed";

}  // namespace

void Controller::begin(uint32_t now) {
  prefs_.begin(PREFS_NAMESPACE, false);
  // Giữ trạng thái An ninh qua lần khởi động lại (mất điện, reset).
  armed_ = prefs_.getBool(PREFS_ARMED, false);
  armActiveAt_ = now + ARM_EXIT_DELAY_MS;
  if (armed_) event("An ninh dang BAT (khoi phuc sau khoi dong)");
}

void Controller::update(uint32_t now) {
  updateAlarms(now);
  updateAutomation(now);
  applyBuzzer();

  if (door_.state() != lastDoorState_) {
    if (door_.state() == DoorState::NotClosed) event("Cua chua dong han: reed switch van bao ho");
    lastDoorState_ = door_.state();
    changed_ = true;
  }
}

CommandResult Controller::openDoorByUser(uint32_t now) {
  door_.open(now);
  changed_ = true;
  event("Mo cua theo lenh");
  // Mở cửa hợp lệ (nhận diện khuôn mặt, nút trên app) nghĩa là chủ nhà đã về: tắt An ninh.
  // Cửa tự mở khi cháy không đi qua hàm này nên không tắt An ninh.
  if (armed_) setArmed(false, now);
  return CommandResult::Ok;
}

CommandResult Controller::closeDoorByUser(uint32_t now) {
  // Đang cháy thì cửa được giữ mở để thoát hiểm.
  if (door_.held()) return CommandResult::DoorHeldByFire;
  door_.close(now);
  changed_ = true;
  event("Dong cua theo lenh");
  return CommandResult::Ok;
}

void Controller::updateAlarms(uint32_t now) {
  const SensorReadings& r = sensors_.readings();

  // MQ-2 có hysteresis để còi không bật/tắt liên tục quanh ngưỡng.
  if (r.mq2Ready) {
    if (!mq2Alarm_ && r.mq2Raw >= MQ2_ALARM_ON) mq2Alarm_ = true;
    if (mq2Alarm_ && r.mq2Raw <= MQ2_ALARM_OFF) mq2Alarm_ = false;
  }
  const bool gas = mq2Alarm_ || simulatedGas_;

  bool heat = heatAlarm_;
  if (r.dhtOk) {
    if (!heat && r.temperatureC >= TEMP_FIRE_ON_C) heat = true;
    if (heat && r.temperatureC <= TEMP_FIRE_OFF_C) heat = false;
  }

  const bool wasFire = fireAlarm();
  if (gas != gasAlarm_ || heat != heatAlarm_) changed_ = true;
  gasAlarm_ = gas;
  heatAlarm_ = heat;

  if (fireAlarm() && !wasFire) {
    fireSilenced_ = false;
    event(gasAlarm_ ? "CANH BAO CHAY: MQ-2 vuot nguong, mo cua thoat hiem"
                    : "CANH BAO CHAY: nhiet do cao, mo cua thoat hiem");
  } else if (!fireAlarm() && wasFire) {
    fireSilenced_ = false;
    event("Het canh bao chay");
  }
  // Cháy: mở và giữ cửa mở để thoát hiểm; hết cháy thì cửa vẫn mở cho tới khi có lệnh đóng.
  door_.setHold(fireAlarm(), now);

  // Ngắt cầu dao mô phỏng: cắt cả hai relay và bỏ lệnh tay đang giữ.
  if (fireAlarm()) {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
      relayOverrides_[i].active = false;
      if (outputs_.relay(i)) {
        outputs_.setRelay(i, false);
        changed_ = true;
      }
    }
  }

  // Cửa bị mở trái phép: servo đang ở vị trí đóng mà reed switch vẫn báo hở (trạng thái NotClosed).
  // Mở cửa bằng lệnh hợp lệ (nhận diện, nút trên app) không tính vì lệnh đó đã tắt An ninh.
  const bool doorForced = door_.state() == DoorState::NotClosed;

  // Báo trộm chỉ kéo dài khi còn chuyển động / cửa còn hở; hết INTRUSION_CLEAR_MS thì tự tắt.
  if (armed_ && timeReached(now, armActiveAt_) && (r.motion || doorForced)) {
    lastIntrusionMotionAt_ = now;
    if (!intrusionAlarm_) {
      intrusionAlarm_ = true;
      intrusionSilenced_ = false;
      doorForced_ = doorForced;
      changed_ = true;
      event(doorForced ? "CANH BAO TROM: cua bi mo trai phep" : "CANH BAO TROM: phat hien chuyen dong");
    }
  } else if (intrusionAlarm_ && now - lastIntrusionMotionAt_ >= INTRUSION_CLEAR_MS) {
    intrusionAlarm_ = false;
    intrusionSilenced_ = false;
    doorForced_ = false;
    changed_ = true;
    event("Het chuyen dong, tat bao trom");
  }
}

void Controller::updateAutomation(uint32_t now) {
  const SensorReadings& r = sensors_.readings();

  // An ninh TẮT: trời tối và có chuyển động -> bật đèn phòng khách; PIR hết báo chuyển động
  // LIVING_LIGHT_OFF_DELAY_MS thì tắt. Chỉ xét LDR lúc bật: khi đèn đã sáng, ánh sáng của
  // chính các LED có thể làm LDR báo "sáng" và làm đèn tắt dù người vẫn còn đó.
  if (armed_) {
    livingAutoOn_ = false;
  } else if (r.motion) {
    if (r.isDark) livingAutoOn_ = true;
    lastMotionAt_ = now;
  } else if (livingAutoOn_ && now - lastMotionAt_ >= LIVING_LIGHT_OFF_DELAY_MS) {
    livingAutoOn_ = false;
  }

  if (r.dhtOk) {
    if (!fanAutoOn_ && r.temperatureC >= FAN_ON_C) fanAutoOn_ = true;
    if (fanAutoOn_ && r.temperatureC <= FAN_OFF_C) fanAutoOn_ = false;
  }

  // Đèn phụ chỉ phản ứng lúc báo trộm bắt đầu/kết thúc; ngoài lúc đó giữ theo lệnh tay.
  if (intrusionAlarm_ != alarmLightOn_) {
    alarmLightOn_ = intrusionAlarm_;
    if (!fireAlarm() && outputs_.relay(RELAY_ALARM_LIGHT) != alarmLightOn_) {
      outputs_.setRelay(RELAY_ALARM_LIGHT, alarmLightOn_);
      changed_ = true;
    }
  }

  applyAutomated(false, LED_PHONG_KHACH, livingAutoOn_, now);
  applyAutomated(false, LED_BAN_CONG, r.isDark, now);
  if (!fireAlarm()) applyAutomated(true, RELAY_FAN, fanAutoOn_, now);
}

void Controller::applyAutomated(bool isRelay, uint8_t index, bool autoValue, uint32_t now) {
  Override& o = isRelay ? relayOverrides_[index] : ledOverrides_[index];
  if (o.active && timeReached(now, o.until)) o.active = false;

  // Cảm biến đổi trạng thái (sáng/tối, vượt ngưỡng...) thì tự động hóa lấy lại quyền ngay,
  // lệnh tay chỉ giữ ưu tiên khi môi trường chưa thay đổi.
  bool& lastAuto = isRelay ? relayLastAuto_[index] : ledLastAuto_[index];
  if (autoValue != lastAuto) {
    lastAuto = autoValue;
    o.active = false;
  }

  const bool target = o.active ? o.value : autoValue;
  const bool current = isRelay ? outputs_.relay(index) : outputs_.led(index);
  if (target == current) return;

  if (isRelay) {
    outputs_.setRelay(index, target);
  } else {
    outputs_.setLed(index, target);
  }
  changed_ = true;
}

void Controller::applyBuzzer() {
  BuzzerPattern pattern = BuzzerPattern::Off;
  if (fireAlarm() && !fireSilenced_) {
    pattern = BuzzerPattern::Fire;
  } else if (intrusionAlarm_ && !intrusionSilenced_) {
    pattern = BuzzerPattern::Intrusion;
  }
  if (pattern != outputs_.buzzerPattern()) {
    outputs_.setBuzzerPattern(pattern);
    changed_ = true;
  }
}

CommandResult Controller::setLedByUser(uint8_t index, bool on, uint32_t now) {
  if (index >= LED_COUNT) return CommandResult::InvalidTarget;
  if (isAutomatedLed(index)) setOverride(ledOverrides_[index], on, now);
  if (outputs_.led(index) != on) changed_ = true;
  outputs_.setLed(index, on);
  return CommandResult::Ok;
}

CommandResult Controller::setRelayByUser(uint8_t index, bool on, uint32_t now) {
  if (index >= RELAY_COUNT) return CommandResult::InvalidTarget;
  if (on && fireAlarm()) return CommandResult::BlockedByFire;
  if (index == RELAY_FAN) setOverride(relayOverrides_[index], on, now);
  if (outputs_.relay(index) != on) changed_ = true;
  outputs_.setRelay(index, on);
  return CommandResult::Ok;
}

void Controller::setArmed(bool armed, uint32_t now) {
  if (armed == armed_) return;
  armed_ = armed;
  prefs_.putBool(PREFS_ARMED, armed_);
  changed_ = true;

  if (armed_) {
    armActiveAt_ = now + ARM_EXIT_DELAY_MS;
    event("An ninh BAT (cho thoi gian roi khoi khu vuc)");
  } else {
    intrusionAlarm_ = false;
    intrusionSilenced_ = false;
    event("An ninh TAT");
  }
}

void Controller::silenceAlarm() {
  // Chỉ tắt còi; trạng thái cảnh báo vẫn giữ để ứng dụng hiển thị.
  if (fireAlarm()) fireSilenced_ = true;
  if (intrusionAlarm_) intrusionSilenced_ = true;
  changed_ = true;
}

void Controller::simulateGas(bool active) {
  simulatedGas_ = active;
  changed_ = true;
}

void Controller::setOverride(Override& o, bool value, uint32_t now) {
  o.active = true;
  o.value = value;
  o.until = now + USER_OVERRIDE_MS;
}

bool Controller::armingPending(uint32_t now) const {
  return armed_ && !timeReached(now, armActiveAt_);
}

bool Controller::alarmSilenced() const {
  return (fireAlarm() && fireSilenced_) || (intrusionAlarm_ && intrusionSilenced_);
}

bool Controller::overrideActive(bool isRelay, uint8_t index, uint32_t now) const {
  const Override& o = isRelay ? relayOverrides_[index] : ledOverrides_[index];
  return o.active && !timeReached(now, o.until);
}

bool Controller::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void Controller::event(const char* message) {
  Serial.print("[SU KIEN] ");
  Serial.println(message);
}
