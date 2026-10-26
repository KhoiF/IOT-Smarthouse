#include "sensors.h"

namespace {

bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

}  // namespace

void Sensors::begin(uint32_t now) {
  bootAt_ = now;

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LDR_DO, INPUT);
  pinMode(PIN_MQ2, INPUT);
  analogReadResolution(12);

  dht_.begin();
  // DHT11 cần khoảng 1 giây sau khi cấp nguồn mới đọc được.
  nextDhtRead_ = now + 1000;

  readings_.isDark = digitalRead(PIN_LDR_DO) == LDR_DARK_LEVEL;
  ldrCandidate_ = readings_.isDark;
  ldrCandidateSince_ = now;
}

void Sensors::update(uint32_t now) {
  readDht(now);
  readMq2(now);
  readLdr(now);

  readings_.pirReady = timeReached(now, bootAt_ + PIR_WARMUP_MS);
  readings_.motion = readings_.pirReady && digitalRead(PIN_PIR) == HIGH;
}

void Sensors::readDht(uint32_t now) {
  if (!timeReached(now, nextDhtRead_)) return;
  nextDhtRead_ = now + DHT_INTERVAL_MS;

  const float t = dht_.readTemperature();
  const float h = dht_.readHumidity();
  if (isnan(t) || isnan(h)) {
    // DHT11 thỉnh thoảng bỏ một lần đọc; chỉ báo lỗi khi lỗi liên tiếp.
    if (dhtFails_ < DHT_MAX_FAILS) dhtFails_++;
    if (dhtFails_ >= DHT_MAX_FAILS) readings_.dhtOk = false;
    return;
  }

  dhtFails_ = 0;
  readings_.temperatureC = t;
  readings_.humidityPct = h;
  readings_.dhtOk = true;
}

void Sensors::readMq2(uint32_t now) {
  if (!timeReached(now, nextMq2Sample_)) return;
  nextMq2Sample_ = now + MQ2_SAMPLE_MS;

  const float raw = analogRead(PIN_MQ2);
  mq2Filtered_ = mq2Filtered_ < 0 ? raw : mq2Filtered_ + MQ2_FILTER_ALPHA * (raw - mq2Filtered_);
  readings_.mq2Raw = (uint16_t)(mq2Filtered_ + 0.5f);
  readings_.mq2Ready = timeReached(now, bootAt_ + MQ2_WARMUP_MS);
}

void Sensors::readLdr(uint32_t now) {
  const bool dark = digitalRead(PIN_LDR_DO) == LDR_DARK_LEVEL;
  if (dark != ldrCandidate_) {
    ldrCandidate_ = dark;
    ldrCandidateSince_ = now;
  }
  if (ldrCandidate_ != readings_.isDark && now - ldrCandidateSince_ >= LDR_DEBOUNCE_MS) {
    readings_.isDark = ldrCandidate_;
  }
}
