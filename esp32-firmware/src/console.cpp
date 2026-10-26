#include "console.h"

#include <string.h>

namespace {

// Nhận số phòng 1..5 hoặc id phòng (vd: phong_khach). Trả LED_COUNT nếu không hợp lệ.
uint8_t parseRoom(const char* arg) {
  if (!arg) return LED_COUNT;
  const int number = atoi(arg);
  if (number >= 1 && number <= LED_COUNT) return number - 1;
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    if (strcmp(arg, ROOM_IDS[i]) == 0) return i;
  }
  return LED_COUNT;
}

// Trả 1 cho "on", 0 cho "off", -1 nếu không hợp lệ.
int parseOnOff(const char* arg) {
  if (!arg) return -1;
  if (strcmp(arg, "on") == 0) return 1;
  if (strcmp(arg, "off") == 0) return 0;
  return -1;
}

const char* onOff(bool value) { return value ? "BAT" : "tat"; }

}  // namespace

void Console::begin() {
  Serial.println();
  Serial.printf("Nha thong minh - firmware %s\n", FIRMWARE_VERSION);
  Serial.println("PIR can ~60 giay, MQ-2 can ~2 phut de on dinh. Go `help` de xem lenh.");
}

void Console::update(uint32_t now) {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buffer_[length_] = '\0';
      if (length_ > 0) handleLine(buffer_, now);
      length_ = 0;
      continue;
    }
    if (length_ < sizeof(buffer_) - 1) buffer_[length_++] = (char)tolower(c);
  }

  if (logEnabled_ && (int32_t)(now - nextLog_) >= 0) {
    nextLog_ = now + STATUS_LOG_INTERVAL_MS;
    printLogLine();
  }
}

void Console::handleLine(char* line, uint32_t now) {
  const char* cmd = strtok(line, " ");
  const char* arg1 = strtok(nullptr, " ");
  const char* arg2 = strtok(nullptr, " ");
  if (!cmd) return;

  if (strcmp(cmd, "help") == 0) {
    printHelp();
  } else if (strcmp(cmd, "status") == 0) {
    printStatus(now);
  } else if (strcmp(cmd, "log") == 0) {
    logEnabled_ = !logEnabled_;
    Serial.printf("Log dinh ky: %s\n", logEnabled_ ? "bat" : "tat");
  } else if (strcmp(cmd, "led") == 0) {
    const uint8_t room = parseRoom(arg1);
    const int on = parseOnOff(arg2);
    if (room >= LED_COUNT || on < 0) {
      Serial.println("Cu phap: led <1-5|id_phong> on|off");
      return;
    }
    printResult(controller_.setLedByUser(room, on == 1, now));
  } else if (strcmp(cmd, "relay") == 0) {
    const int index = arg1 ? atoi(arg1) : 0;
    const int on = parseOnOff(arg2);
    if (index < 1 || index > RELAY_COUNT || on < 0) {
      Serial.println("Cu phap: relay <1-2> on|off");
      return;
    }
    printResult(controller_.setRelayByUser(index - 1, on == 1, now));
  } else if (strcmp(cmd, "door") == 0 && arg1 && strcmp(arg1, "open") == 0) {
    printResult(controller_.openDoorByUser(now));
  } else if (strcmp(cmd, "arm") == 0) {
    controller_.setArmed(true, now);
  } else if (strcmp(cmd, "disarm") == 0) {
    controller_.setArmed(false, now);
  } else if (strcmp(cmd, "silence") == 0) {
    controller_.silenceAlarm();
    Serial.println("Da tat coi (canh bao van con hieu luc)");
  } else if (strcmp(cmd, "buzz") == 0 && arg1 &&
             (strcmp(arg1, "high") == 0 || strcmp(arg1, "low") == 0)) {
    // Ghi thẳng mức lên chân còi để dò mức kích; bị ghi đè khi trạng thái cảnh báo đổi.
    const bool high = strcmp(arg1, "high") == 0;
    digitalWrite(PIN_BUZZER, high ? HIGH : LOW);
    Serial.printf("GPIO%u = %s\n", PIN_BUZZER, high ? "HIGH" : "LOW");
  } else if (strcmp(cmd, "sim") == 0 && arg1 && strcmp(arg1, "gas") == 0) {
    const int on = parseOnOff(arg2);
    if (on < 0) {
      Serial.println("Cu phap: sim gas on|off");
      return;
    }
    controller_.simulateGas(on == 1);
    Serial.printf("Gia lap gas: %s\n", on ? "bat" : "tat");
  } else {
    Serial.println("Lenh khong hop le. Go `help`.");
  }
}

void Console::printHelp() {
  Serial.println("Lenh:");
  Serial.println("  status                  in toan bo trang thai");
  Serial.println("  log                     bat/tat dong log moi 2 giay");
  Serial.println("  led <1-5> on|off        1=phong khach 2=phong ngu 3=nha bep 4=ban cong 5=nha tam");
  Serial.println("  relay <1-2> on|off      1=quat 2=den");
  Serial.println("  door open               mo cua (tu dong sau 5 giay)");
  Serial.println("  arm | disarm            bat/tat che do An ninh");
  Serial.println("  silence                 tat coi dang keu");
  Serial.println("  sim gas on|off          gia lap MQ-2 vuot nguong (khong can khoi that)");
  Serial.println("  buzz high|low           ghi thang muc len chan coi (do muc kich)");
}

void Console::printStatus(uint32_t now) {
  const SensorReadings& r = sensors_.readings();

  Serial.println("----- TRANG THAI -----");
  if (r.dhtOk) {
    Serial.printf("DHT11      : %.1f C, %.1f %%\n", r.temperatureC, r.humidityPct);
  } else {
    Serial.println("DHT11      : loi doc");
  }
  Serial.printf("MQ-2       : %u%s (bao >= %.0f, het <= %.0f)\n", r.mq2Raw,
                r.mq2Ready ? "" : " [dang warm-up]", MQ2_ALARM_ON, MQ2_ALARM_OFF);
  Serial.printf("Anh sang   : %s (DO=%d, toi khi DO=%d)\n", r.isDark ? "TOI" : "sang",
                digitalRead(PIN_LDR_DO), LDR_DARK_LEVEL);
  Serial.printf("Chuyen dong: %s%s\n", r.motion ? "CO" : "khong",
                r.pirReady ? "" : " [PIR dang khoi dong]");

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    Serial.printf("LED %u %-11s: %s%s\n", i + 1, ROOM_IDS[i], onOff(outputs_.led(i)),
                  controller_.overrideActive(false, i, now) ? " (lenh tay)" : "");
  }
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    Serial.printf("Relay %u %-9s: %s%s\n", i + 1, i == RELAY_FAN ? "quat" : "den",
                  onOff(outputs_.relay(i)),
                  controller_.overrideActive(true, i, now) ? " (lenh tay)" : "");
  }

  Serial.printf("Cua       : %s%s%s\n", Door::stateName(door_.state()), door_.held() ? " (giu mo do chay)" : "",
                DOOR_SENSOR_ENABLED ? (door_.sensorClosed() ? ", reed: dong" : ", reed: HO") : ", reed: chua lap");
  Serial.printf("An ninh    : %s%s\n", controller_.armed() ? "BAT" : "tat",
                controller_.armingPending(now) ? " (dang cho roi khu vuc)" : "");
  Serial.printf("Canh bao   : chay=%s (gas=%s%s, nhiet=%s) trom=%s%s\n",
                onOff(controller_.fireAlarm()), onOff(controller_.gasAlarm()),
                controller_.gasSimulated() ? " gia lap" : "", onOff(controller_.heatAlarm()),
                onOff(controller_.intrusionAlarm()),
                controller_.alarmSilenced() ? " [da tat coi]" : "");
  Serial.println("----------------------");
}

void Console::printLogLine() {
  const SensorReadings& r = sensors_.readings();

  char leds[LED_COUNT + 1];
  for (uint8_t i = 0; i < LED_COUNT; i++) leds[i] = outputs_.led(i) ? '1' : '0';
  leds[LED_COUNT] = '\0';

  Serial.printf("T=%s H=%s MQ2=%u%s toi=%d cd=%d | LED=%s R=%d%d coi=%d | an_ninh=%d chay=%d trom=%d\n",
                r.dhtOk ? String(r.temperatureC, 1).c_str() : "--",
                r.dhtOk ? String(r.humidityPct, 0).c_str() : "--", r.mq2Raw,
                r.mq2Ready ? "" : "*", r.isDark, r.motion, leds, outputs_.relay(0),
                outputs_.relay(1), outputs_.buzzerPattern() != BuzzerPattern::Off,
                controller_.armed(), controller_.fireAlarm(), controller_.intrusionAlarm());
}

void Console::printResult(CommandResult result) {
  switch (result) {
    case CommandResult::Ok:
      Serial.println("OK");
      break;
    case CommandResult::InvalidTarget:
      Serial.println("Loi: thiet bi khong hop le");
      break;
    case CommandResult::BlockedByFire:
      Serial.println("Loi: dang canh bao chay, khong bat relay");
      break;
  }
}
