#pragma once

// Sao chép file này thành secrets.h rồi điền thông tin. secrets.h không được commit.

#define WIFI_SSID "ten-wifi"          // ESP32 chỉ bắt được Wi-Fi 2,4 GHz
#define WIFI_PASSWORD "mat-khau-wifi"

// IP LAN của máy chạy backend (macOS: ipconfig getifaddr en0). Không dùng localhost.
#define BACKEND_HOST "192.168.1.30"
#define BACKEND_PORT 8000

#define DEVICE_ID "esp32-home-1"
// Trùng IOT_DEVICE_TOKEN trong backend/.env; để trống nếu backend không đặt token.
#define DEVICE_TOKEN ""
