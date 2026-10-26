#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "controller.h"
#include "door.h"
#include "outputs.h"
#include "sensors.h"

// Kết nối với backend FastAPI:
// - Telemetry định kỳ qua HTTP POST /iot/telemetry.
// - WebSocket /ws/iot/device: gửi hello, nhận command, trả ack; thay đổi tức thời gửi
//   bằng message "state" khi socket đang mở.
// Mất mạng không ảnh hưởng logic cục bộ trong Controller.
class ServerLink {
 public:
  ServerLink(Controller& controller, Sensors& sensors, Outputs& outputs, Door& door)
      : controller_(controller), sensors_(sensors), outputs_(outputs), door_(door) {}

  void begin();
  void update(uint32_t now);

 private:
  void updateWifi(uint32_t now);
  void updateTelemetry(uint32_t now);
  void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void handleCommand(const uint8_t* payload, size_t length);
  void sendHello();
  void sendStateOverWs();
  bool postTelemetry();
  void fillState(JsonObject state);
  void sendJson(JsonDocument& doc);

  Controller& controller_;
  Sensors& sensors_;
  Outputs& outputs_;
  Door& door_;
  WebSocketsClient ws_;

  bool wifiConnected_ = false;
  bool wsConnected_ = false;
  uint32_t nextWifiAttempt_ = 0;

  uint32_t nextTelemetry_ = 0;
  uint32_t lastPush_ = 0;
  uint32_t telemetryBackoffUntil_ = 0;
  int lastHttpCode_ = 0;
  bool pendingChange_ = true;
  bool lastMotion_ = false;
  bool lastDark_ = false;
};
