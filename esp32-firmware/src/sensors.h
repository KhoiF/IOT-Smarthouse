#pragma once

#include <Arduino.h>
#include <DHT.h>

#include "config.h"

struct SensorReadings {
  float temperatureC = NAN;
  float humidityPct = NAN;
  bool dhtOk = false;

  uint16_t mq2Raw = 0;     // giá trị ADC đã lọc, 0..4095
  bool mq2Ready = false;   // đã qua thời gian warm-up

  bool motion = false;
  bool pirReady = false;   // đã qua thời gian khởi động của PIR

  bool isDark = false;     // đã chống dội
};

class Sensors {
 public:
  Sensors() : dht_(PIN_DHT, DHT11) {}

  void begin(uint32_t now);
  void update(uint32_t now);

  const SensorReadings& readings() const { return readings_; }

 private:
  void readDht(uint32_t now);
  void readMq2(uint32_t now);
  void readLdr(uint32_t now);

  DHT dht_;
  SensorReadings readings_;

  uint32_t bootAt_ = 0;
  uint32_t nextDhtRead_ = 0;
  uint8_t dhtFails_ = 0;

  uint32_t nextMq2Sample_ = 0;
  float mq2Filtered_ = -1;

  bool ldrCandidate_ = false;
  uint32_t ldrCandidateSince_ = 0;
};
