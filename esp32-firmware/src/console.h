#pragma once

#include <Arduino.h>

#include "controller.h"
#include "door.h"
#include "outputs.h"
#include "sensors.h"

// Lệnh qua Serial Monitor để thử phần cứng khi chưa có mạng. Gõ `help` để xem lệnh.
class Console {
 public:
  Console(Controller& controller, Sensors& sensors, Outputs& outputs, Door& door)
      : controller_(controller), sensors_(sensors), outputs_(outputs), door_(door) {}

  void begin();
  void update(uint32_t now);

 private:
  void handleLine(char* line, uint32_t now);
  void printHelp();
  void printStatus(uint32_t now);
  void printLogLine();
  void printResult(CommandResult result);

  Controller& controller_;
  Sensors& sensors_;
  Outputs& outputs_;
  Door& door_;

  char buffer_[64] = {};
  uint8_t length_ = 0;
  bool logEnabled_ = true;
  uint32_t nextLog_ = 0;
};
