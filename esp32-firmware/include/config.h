#pragma once

#include <Arduino.h>

// Cấu hình phần cứng và ngưỡng. Đổi chân ở đây thì phải cập nhật cả
// IOT_COMPLETION_GUIDE.md và backend (danh sách phòng).

constexpr const char* FIRMWARE_VERSION = "0.1.0";

// ---------------------------------------------------------------------------
// LED: thứ tự là thứ tự phòng, id dùng chung với backend/web/Android.
// ---------------------------------------------------------------------------
constexpr uint8_t LED_COUNT = 5;
constexpr uint8_t LED_PINS[LED_COUNT] = {18, 19, 21, 22, 23};
constexpr const char* ROOM_IDS[LED_COUNT] = {
    "phong_khach", "phong_ngu", "nha_bep", "ban_cong", "nha_tam"};

constexpr uint8_t LED_PHONG_KHACH = 0;
constexpr uint8_t LED_BAN_CONG = 3;

// ---------------------------------------------------------------------------
// Relay: relay 1 = quạt 5 V, relay 2 = đèn 3,3 V.
// ---------------------------------------------------------------------------
constexpr uint8_t RELAY_COUNT = 2;
constexpr uint8_t RELAY_PINS[RELAY_COUNT] = {25, 26};
constexpr uint8_t RELAY_FAN = 0;
// Phần lớn module relay kích mức thấp. Đổi thành false nếu relay đóng khi IN = HIGH.
constexpr bool RELAY_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// Buzzer active trên GPIO27.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_BUZZER = 27;
// Buzzer đang dùng kêu khi chân ở mức LOW (đã thử trên mạch).
constexpr bool BUZZER_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// Cảm biến
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_DHT = 32;
constexpr uint8_t PIN_PIR = 33;
constexpr uint8_t PIN_MQ2 = 34;     // ADC1, input-only. Hiện cấp MQ-2 bằng 3V3.
constexpr uint8_t PIN_LDR_DO = 35;  // Module LDR chỉ có DO, không có AO.

// Module LDR (LM393) đang dùng cho DO = HIGH khi tối (đã thử trên mạch).
constexpr uint8_t LDR_DARK_LEVEL = HIGH;

// ---------------------------------------------------------------------------
// Thời gian (ms)
// ---------------------------------------------------------------------------
constexpr uint32_t DHT_INTERVAL_MS = 2000;
constexpr uint8_t DHT_MAX_FAILS = 3;         // số lần đọc lỗi liên tiếp trước khi báo dht_ok = false
constexpr uint32_t MQ2_SAMPLE_MS = 100;
constexpr uint32_t MQ2_WARMUP_MS = 120000;   // chưa xét cảnh báo gas khi MQ-2 còn nóng lên
constexpr uint32_t PIR_WARMUP_MS = 60000;    // PIR báo sai trong lúc khởi động
constexpr uint32_t LDR_DEBOUNCE_MS = 2000;   // DO dao động khi sáng gần ngưỡng

constexpr uint32_t LIVING_LIGHT_OFF_DELAY_MS = 3000;  // PIR hết báo chuyển động 3 giây thì tắt LED phòng khách
constexpr uint32_t USER_OVERRIDE_MS = 300000;     // lệnh tay/AI giữ ưu tiên 5 phút
constexpr uint32_t INTRUSION_CLEAR_MS = 2000;     // hết chuyển động 2 giây thì tắt còi báo trộm
constexpr uint32_t ARM_EXIT_DELAY_MS = 2000;      // thời gian chờ sau khi bật An ninh trước khi PIR được xét

// ---------------------------------------------------------------------------
// Ngưỡng
// ---------------------------------------------------------------------------
// MQ-2 đang cấp 3V3 và chưa hiệu chuẩn: đo giá trị nền bằng lệnh `status`
// rồi chỉnh lại. Đổi sang 5 V + chia áp thì phải đo lại toàn bộ.
constexpr float MQ2_ALARM_ON = 3600;
constexpr float MQ2_ALARM_OFF = 3300;
constexpr float MQ2_FILTER_ALPHA = 0.1f;  // lọc trung bình mũ cho ADC

// DHT11 chỉ đo tới khoảng 50 °C.
constexpr float TEMP_FIRE_ON_C = 45.0f;
constexpr float TEMP_FIRE_OFF_C = 42.0f;

// Quạt tự động. Phòng đang khoảng 31 °C nên ngưỡng 30 °C sẽ làm quạt chạy liên tục.
constexpr float FAN_ON_C = 33.0f;
constexpr float FAN_OFF_C = 31.0f;

// ---------------------------------------------------------------------------
// Còi (ms bật / ms tắt)
// ---------------------------------------------------------------------------
constexpr uint32_t BUZZER_INTRUSION_ON_MS = 400;
constexpr uint32_t BUZZER_INTRUSION_OFF_MS = 400;
constexpr uint32_t BUZZER_FIRE_ON_MS = 120;
constexpr uint32_t BUZZER_FIRE_OFF_MS = 120;

constexpr uint32_t STATUS_LOG_INTERVAL_MS = 2000;

// ---------------------------------------------------------------------------
// Mạng
// ---------------------------------------------------------------------------
constexpr const char* RELAY_IDS[RELAY_COUNT] = {"relay1", "relay2"};

constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 2000;
constexpr uint32_t TELEMETRY_MIN_GAP_MS = 300;      // gửi ngay khi có thay đổi nhưng không dồn dập
constexpr uint32_t TELEMETRY_BACKOFF_MS = 10000;    // HTTP lỗi thì tạm ngừng để không chặn vòng lặp
constexpr uint16_t HTTP_TIMEOUT_MS = 1500;
constexpr uint32_t WS_RECONNECT_MS = 5000;
