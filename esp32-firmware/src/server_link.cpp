#include "server_link.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_system.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "Chua co include/secrets.h: dang dung secrets.example.h, ESP32 se khong vao duoc Wi-Fi"
#include "secrets.example.h"
#endif

namespace {

bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

int indexOf(const char* const* ids, uint8_t count, const char* id) {
  for (uint8_t i = 0; i < count; i++) {
    if (strcmp(ids[i], id) == 0) return i;
  }
  return -1;
}

// Lý do khởi động lại gần nhất, để backend ghi log khi ESP32 kết nối lại (mất nguồn, sụt áp...).
const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "power_on";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_BROWNOUT:
      return "brownout";
    default:
      return "other";
  }
}

const char* errorCode(CommandResult result) {
  switch (result) {
    case CommandResult::Ok:
      return nullptr;
    case CommandResult::InvalidTarget:
      return "invalid_target";
    case CommandResult::BlockedByFire:
      return "blocked_by_fire";
    case CommandResult::DoorHeldByFire:
      return "door_held_by_fire";
  }
  return "bad_request";
}

}  // namespace

void ServerLink::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // giảm độ trễ khi nhận lệnh
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  nextWifiAttempt_ = millis() + WIFI_RETRY_MS;
  Serial.printf("[MANG] Dang ket noi Wi-Fi \"%s\", backend %s:%d\n", WIFI_SSID, BACKEND_HOST,
                BACKEND_PORT);

  ws_.begin(BACKEND_HOST, BACKEND_PORT, "/ws/iot/device");
  ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    onWsEvent(type, payload, length);
  });
  ws_.setReconnectInterval(WS_RECONNECT_MS);
  ws_.enableHeartbeat(15000, 3000, 2);
}

void ServerLink::update(uint32_t now) {
  updateWifi(now);
  if (!wifiConnected_) return;
  ws_.loop();
  updateTelemetry(now);
}

void ServerLink::updateWifi(uint32_t now) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiConnected_) {
    wifiConnected_ = connected;
    if (connected) {
      Serial.printf("[MANG] Da vao Wi-Fi, IP %s\n", WiFi.localIP().toString().c_str());
      nextTelemetry_ = now;
      telemetryBackoffUntil_ = now;
    } else {
      Serial.println("[MANG] Mat Wi-Fi, logic cuc bo van chay");
      wsConnected_ = false;
    }
  }

  if (!connected && timeReached(now, nextWifiAttempt_)) {
    nextWifiAttempt_ = now + WIFI_RETRY_MS;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void ServerLink::updateTelemetry(uint32_t now) {
  const SensorReadings& r = sensors_.readings();
  if (r.motion != lastMotion_ || r.isDark != lastDark_) {
    lastMotion_ = r.motion;
    lastDark_ = r.isDark;
    pendingChange_ = true;
  }
  if (controller_.consumeChanged()) pendingChange_ = true;

  // Thay đổi tức thời: đẩy qua WebSocket (không chặn vòng lặp như HTTP).
  if (pendingChange_ && wsConnected_ && now - lastPush_ >= TELEMETRY_MIN_GAP_MS) {
    pendingChange_ = false;
    lastPush_ = now;
    sendStateOverWs();
  }

  if (!timeReached(now, nextTelemetry_) || !timeReached(now, telemetryBackoffUntil_)) return;
  nextTelemetry_ = now + TELEMETRY_INTERVAL_MS;
  if (postTelemetry()) {
    pendingChange_ = false;
  } else {
    telemetryBackoffUntil_ = now + TELEMETRY_BACKOFF_MS;
  }
}

bool ServerLink::postTelemetry() {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  const String url = String("http://") + BACKEND_HOST + ":" + BACKEND_PORT + "/iot/telemetry";
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(DEVICE_TOKEN) > 0) http.addHeader("X-Device-Token", DEVICE_TOKEN);

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  fillState(doc["state"].to<JsonObject>());
  String body;
  serializeJson(doc, body);

  const int code = http.POST(body);
  http.end();

  const bool ok = code >= 200 && code < 300;
  if (code != lastHttpCode_) {
    // Chỉ in khi kết quả đổi để không làm ngập Serial.
    if (ok) {
      Serial.println("[MANG] Gui telemetry thanh cong");
    } else {
      Serial.printf("[MANG] Gui telemetry loi (%d), thu lai sau %lus\n", code,
                    (unsigned long)(TELEMETRY_BACKOFF_MS / 1000));
    }
    lastHttpCode_ = code;
  }
  return ok;
}

void ServerLink::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected_ = true;
      Serial.println("[MANG] Da ket noi WebSocket voi backend");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      if (wsConnected_) Serial.println("[MANG] Mat ket noi WebSocket, dang thu lai");
      wsConnected_ = false;
      break;
    case WStype_TEXT:
      handleCommand(payload, length);
      break;
    default:
      break;
  }
}

void ServerLink::handleCommand(const uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  if (strcmp(doc["type"] | "", "command") != 0) return;

  const uint32_t now = millis();
  const char* action = doc["action"] | "";
  const char* error = nullptr;

  if (strcmp(action, "set_led") == 0 || strcmp(action, "set_relay") == 0) {
    const bool isLed = action[4] == 'l';
    const int index = isLed ? indexOf(ROOM_IDS, LED_COUNT, doc["id"] | "")
                            : indexOf(RELAY_IDS, RELAY_COUNT, doc["id"] | "");
    if (!doc["on"].is<bool>()) {
      error = "bad_request";
    } else if (index < 0) {
      error = "invalid_target";
    } else {
      const bool on = doc["on"].as<bool>();
      error = errorCode(isLed ? controller_.setLedByUser(index, on, now)
                              : controller_.setRelayByUser(index, on, now));
    }
  } else if (strcmp(action, "set_armed") == 0) {
    if (doc["armed"].is<bool>()) {
      controller_.setArmed(doc["armed"].as<bool>(), now);
    } else {
      error = "bad_request";
    }
  } else if (strcmp(action, "open_door") == 0) {
    error = errorCode(controller_.openDoorByUser(now));
  } else if (strcmp(action, "close_door") == 0) {
    error = errorCode(controller_.closeDoorByUser(now));
  } else if (strcmp(action, "silence") == 0) {
    controller_.silenceAlarm();
  } else {
    error = "unknown_action";
  }

  // Áp dụng ngay (còi, tự động hóa) để ACK mang trạng thái thật sau lệnh.
  controller_.update(now);
  Serial.printf("[MANG] Lenh %s %s -> %s\n", action, doc["id"] | "", error ? error : "OK");

  JsonDocument ack;
  ack["type"] = "ack";
  ack["command_id"] = doc["command_id"] | "";
  ack["ok"] = error == nullptr;
  if (error) ack["error"] = error;
  fillState(ack["state"].to<JsonObject>());
  sendJson(ack);
  controller_.consumeChanged();  // trạng thái mới đã nằm trong ACK
}

void ServerLink::sendHello() {
  JsonDocument doc;
  doc["type"] = "hello";
  doc["device_id"] = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["token"] = DEVICE_TOKEN;
  doc["reset_reason"] = resetReason();
  doc["boot_uptime_ms"] = millis();
  fillState(doc["state"].to<JsonObject>());
  sendJson(doc);
}

void ServerLink::sendStateOverWs() {
  JsonDocument doc;
  doc["type"] = "state";
  fillState(doc["state"].to<JsonObject>());
  sendJson(doc);
}

void ServerLink::sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  ws_.sendTXT(out);
}

void ServerLink::fillState(JsonObject state) {
  const uint32_t now = millis();
  const SensorReadings& r = sensors_.readings();

  state["uptime_ms"] = now;
  state["rssi"] = WiFi.RSSI();

  JsonObject sensors = state["sensors"].to<JsonObject>();
  if (r.dhtOk) {
    sensors["temperature_c"] = r.temperatureC;
    sensors["humidity_pct"] = r.humidityPct;
  } else {
    sensors["temperature_c"] = nullptr;
    sensors["humidity_pct"] = nullptr;
  }
  sensors["dht_ok"] = r.dhtOk;
  sensors["mq2_raw"] = r.mq2Raw;
  sensors["mq2_ready"] = r.mq2Ready;
  sensors["motion"] = r.motion;
  sensors["pir_ready"] = r.pirReady;
  sensors["is_dark"] = r.isDark;

  JsonObject outputs = state["outputs"].to<JsonObject>();
  JsonObject leds = outputs["leds"].to<JsonObject>();
  for (uint8_t i = 0; i < LED_COUNT; i++) leds[ROOM_IDS[i]] = outputs_.led(i);
  JsonObject relays = outputs["relays"].to<JsonObject>();
  for (uint8_t i = 0; i < RELAY_COUNT; i++) relays[RELAY_IDS[i]] = outputs_.relay(i);
  outputs["buzzer"] = outputs_.buzzerPattern() != BuzzerPattern::Off;

  JsonObject security = state["security"].to<JsonObject>();
  security["armed"] = controller_.armed();
  security["arming"] = controller_.armingPending(now);

  JsonObject door = state["door"].to<JsonObject>();
  door["state"] = Door::stateName(door_.state());
  door["held_by_fire"] = door_.held();
  if (DOOR_SENSOR_ENABLED) {
    door["sensor_closed"] = door_.sensorClosed();
  } else {
    door["sensor_closed"] = nullptr;
  }

  JsonObject alarms = state["alarms"].to<JsonObject>();
  alarms["fire"] = controller_.fireAlarm();
  alarms["gas"] = controller_.gasAlarm();
  alarms["heat"] = controller_.heatAlarm();
  alarms["intrusion"] = controller_.intrusionAlarm();
  alarms["door_forced"] = controller_.doorForced();
  alarms["silenced"] = controller_.alarmSilenced();
  alarms["gas_simulated"] = controller_.gasSimulated();
}
