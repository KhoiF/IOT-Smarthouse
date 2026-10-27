# IoT Nhà Thông Minh

Mô hình nhà thông minh thu nhỏ dùng **ESP32** đọc cảm biến và điều khiển thiết bị, **FastAPI** làm máy chủ trung tâm, giao diện **Web (React)** để giám sát/điều khiển, và **trợ lý giọng nói Gemini Live** nói tiếng Việt. Có thêm cửa ra vào mở bằng **nhận diện khuôn mặt**.

ESP32 nói chuyện trực tiếp với backend bằng HTTP và WebSocket trong mạng LAN (không dùng MQTT hay dịch vụ đám mây). Logic an toàn (báo cháy, báo trộm, cửa thoát hiểm) chạy ngay trên ESP32 nên vẫn hoạt động khi mất mạng.

## Chức năng

| Nhóm | Chức năng |
|---|---|
| **Điều khiển** | Bật/tắt 5 đèn (Phòng khách, Phòng ngủ, Nhà bếp, Ban công, Nhà tắm), quạt (relay 1), đèn phụ (relay 2) từ Web hoặc giọng nói. Giao diện chỉ báo "đã làm" khi ESP32 xác nhận (ACK) |
| **Giám sát** | Nhiệt độ, độ ẩm (DHT11), sáng/tối (LDR), chuyển động (PIR), khói/gas (MQ-2), trạng thái ESP32 online/offline; biểu đồ nhiệt độ/độ ẩm 1 giờ gần nhất |
| **An ninh** | Bật chế độ An ninh → có chuyển động thì hú còi và bật đèn phụ, hết chuyển động thì tự tắt; cửa bị mở trái phép cũng báo trộm |
| **Báo cháy** | MQ-2 vượt ngưỡng hoặc nhiệt độ quá cao → còi, cảnh báo trên Web, ngắt cả hai relay, mở cửa thoát hiểm |
| **Tự động hóa** | Trời tối → bật đèn Ban công; tối + có người (An ninh tắt) → bật đèn Phòng khách, hết chuyển động 3 giây thì tắt; nóng ≥ 33 °C → bật quạt. Lệnh tay giữ ưu tiên tới khi cảm biến đổi trạng thái (tối đa 5 phút) |
| **Cửa ra vào** | Nhận diện khuôn mặt thành viên (OpenCV) để mở cửa; nút Mở/Đóng cửa trên Web; mở cửa hợp lệ tự tắt An ninh; cảnh báo "cửa chưa đóng hẳn" khi có cảm biến cửa |
| **Trợ lý giọng nói** | Gemini Live tiếng Việt: bật/tắt đèn, phát/tạm dừng/tiếp tục nhạc, bật An ninh, đóng cửa, hỏi nhiệt độ/độ ẩm/trạng thái nhà. **Không** cho tắt An ninh hoặc mở cửa bằng giọng nói |
| **Khác** | Phát nhạc từ thư viện `backend/music`, phát hiện lửa qua ảnh camera (OpenCV), app Android (đèn, nhạc, chat) |

## Kiến trúc

```
 Web (React) / Android ──REST + WebSocket──►  FastAPI (backend/)  ◄──HTTP telemetry + WebSocket lệnh/ACK──  ESP32 (esp32-firmware/)
        │  micro, camera                      │  iot_service: trạng thái thiết bị, chờ ACK         │  cảm biến: DHT11, PIR, LDR, MQ-2
        └──── /ws/gemini ───────────────────► │  gemini_service: trợ lý giọng nói + tool           │  đầu ra: 5 LED, 2 relay, còi, servo cửa
                                              │  face_service / door_service: nhận diện, mở cửa    │  logic cục bộ: cháy, An ninh, tự động hóa
                                              └──► Google Gemini Live API
```

```
backend/          FastAPI: app/api/routes (API), app/services (logic), app/core (cấu hình, danh sách phòng)
frontend/         Web React + Vite + Tailwind
esp32-firmware/   Firmware PlatformIO (Arduino framework); pin map và ngưỡng ở include/config.h
android-app/      App Android (Kotlin, Jetpack Compose)
HUONG_DAN_DAU_NOI.md      Hướng dẫn đấu dây từng thiết bị, thử bằng Serial, lỗi thường gặp
IOT_COMPLETION_GUIDE.md   Cẩm nang dự án: phần cứng, pin map, hợp đồng API, lộ trình
```

## Phần cứng

ESP32 DevKit V1, breadboard, 5 LED + điện trở 220–330 Ω, DHT11, PIR HC-SR501, MQ-2, module LDR (DO), còi active, relay 2 kênh, quạt mini 5 V, đèn 3,3 V; cửa dùng servo SG90 và reed switch.

| Thiết bị | Chân ESP32 | Nguồn |
|---|---|---|
| LED Phòng khách / Phòng ngủ / Nhà bếp / Ban công / Nhà tắm | GPIO 18 / 19 / 21 / 22 / 23 | – |
| DHT11 | GPIO 32 | 3V3 |
| PIR | GPIO 33 | 5V (VIN) |
| MQ-2 (AO) | GPIO 34 | 3V3 (tạm thời) |
| LDR (DO) | GPIO 35 | 3V3 |
| Còi | GPIO 27 | 3V3 |
| Relay 1 (quạt) / Relay 2 (đèn phụ) | GPIO 25 / 26 | 5V (VIN) |
| Servo cửa | GPIO 13 | 5V (VIN + tụ 470 µF, hoặc adapter riêng) |
| Reed switch cửa | GPIO 16 (chân kia nối GND) | – |

Cách đấu chi tiết, kiểm tra trước khi cấp nguồn và cách thử từng khối: **[HUONG_DAN_DAU_NOI.md](HUONG_DAN_DAU_NOI.md)**. Không đưa tín hiệu 5 V vào GPIO và không đấu điện lưới 220 V vào mô hình.

## Yêu cầu

- macOS hoặc Linux, **Python 3.12**, **Node.js 20+**
- **PlatformIO** để nạp firmware (extension *PlatformIO IDE* cho VS Code, hoặc `pip install platformio`)
- **API key Google AI Studio** cho trợ lý giọng nói (không có key thì phần còn lại vẫn chạy, chat chạy chế độ ngoại tuyến)
- Máy chạy backend và ESP32 dùng **cùng mạng Wi‑Fi 2,4 GHz**

## Cài đặt và chạy

Các lệnh chạy từ thư mục gốc của dự án.

### 1. Backend

```bash
# Tạo môi trường Python 3.12 và cài thư viện
python3.12 -m venv backend/.venv
backend/.venv/bin/pip install -r requirements.txt

# Cấu hình: sao chép rồi điền GOOGLE_API_KEY
cp backend/.env.example backend/.env

# Tải mô hình nhận diện khuôn mặt (~38 MB, một lần)
cd backend && .venv/bin/python -m app.scripts.download_face_models && cd ..
```

Biến chính trong `backend/.env`:

| Biến | Ý nghĩa |
|---|---|
| `GOOGLE_API_KEY` | Key Google AI Studio cho Gemini Live |
| `GEMINI_MODEL` | Model Live, mặc định `gemini-3.8-live` |
| `IOT_DEVICE_TOKEN` | Token chung với `DEVICE_TOKEN` của ESP32 (để trống = không kiểm tra) |
| `FACE_MATCH_THRESHOLD` | Ngưỡng giống nhau để nhận là thành viên (mặc định 0,363; tăng để khắt khe hơn) |

### 2. Frontend

```bash
cd frontend && npm install && cd ..
```

### 3. Firmware ESP32

```bash
cp esp32-firmware/include/secrets.example.h esp32-firmware/include/secrets.h
```

Điền vào `secrets.h`:

```c
#define WIFI_SSID "ten-wifi-2.4GHz"
#define WIFI_PASSWORD "mat-khau"
#define BACKEND_HOST "192.168.1.30"   // IP LAN của máy chạy backend (macOS: ipconfig getifaddr en0), không dùng localhost
#define BACKEND_PORT 8000
```

Cắm ESP32 bằng cáp USB có truyền dữ liệu rồi nạp:

```bash
cd esp32-firmware
pio run -t upload        # nếu "pio" không có trong PATH: ~/.platformio/penv/bin/pio
pio device monitor       # Serial 115200; gõ `help` để xem lệnh thử
```

Trong VS Code có thể dùng thanh PlatformIO ở dưới cùng: ✓ (Build), → (Upload), 🔌 (Serial Monitor). Đóng Serial Monitor trước khi nạp, nếu không sẽ báo cổng bận.

Khi ESP32 vào được mạng, Serial in `[MANG] Da vao Wi-Fi` và `[MANG] Da ket noi WebSocket voi backend`.

### 4. Chạy hệ thống

```bash
./start.sh
```

Script bật backend (cổng **8000**) và Web (cổng **5173**). Mở **http://localhost:5173**. Card "Điều khiển thiết bị" phải hiện **ESP32 online**.

Chạy riêng từng phần (hai cửa sổ terminal):

```bash
cd backend && .venv/bin/uvicorn app.main:app --reload --timeout-graceful-shutdown 2 --host 0.0.0.0 --port 8000
cd frontend && npm run dev
```

Lần đầu macOS có thể hỏi cho Python nhận kết nối mạng: chọn **Allow**, nếu không ESP32 không gửi được dữ liệu lên backend.

## Sử dụng

- **Điều khiển và giám sát:** card *Điều khiển thiết bị* (đèn, relay, chế độ An ninh, cảnh báo) và *Giám sát môi trường* (số liệu, biểu đồ, bảng).
- **Giọng nói:** card *Trò chuyện với Gemini* → **Bật micro** → nói, ví dụ "bật đèn phòng khách", "nhiệt độ phòng bao nhiêu?", "bật chế độ an ninh", "đóng cửa", "phát bài Phép Màu", "tạm dừng nhạc".
- **Cửa và khuôn mặt:** card *Cửa ra vào* → chọn camera (webcam hoặc camera USB) → **Bật camera**.
  - Tab *Thành viên*: nhập tên, **Chụp mẫu** 3–10 lần, **Lưu thành viên**.
  - Tab *Nhận diện*: thành viên đứng trước camera khoảng 1,5 giây thì cửa mở; nút **Mở cửa** / **Đóng cửa** điều khiển tay.
- **Thử phần cứng không cần mạng:** Serial Monitor với các lệnh `status`, `led 1 on`, `relay 1 on`, `arm`, `disarm`, `door open`, `door close`, `sim gas on` (giả lập cháy, không cần khói thật). Xem bảng đầy đủ trong [HUONG_DAN_DAU_NOI.md](HUONG_DAN_DAU_NOI.md#6-thử-từng-khối-bằng-serial-monitor).

Micro và camera chỉ dùng được khi mở trang bằng `localhost` (trên chính máy chạy backend) hoặc HTTPS; trình duyệt chặn chúng khi mở bằng `http://<IP>` từ điện thoại/máy khác. Các chức năng còn lại dùng được từ mọi thiết bị trong mạng qua `http://<IP-máy-chạy-backend>:5173`.

## Docker (tùy chọn)

```bash
docker compose up --build    # backend :8000, web (nginx) :5173
```

Image backend sao chép cả thư mục `backend/`, nên cần có sẵn `backend/.env` và mô hình khuôn mặt trong `backend/models/` trước khi build. Khi chạy trong Docker, ESP32 vẫn trỏ `BACKEND_HOST` tới IP LAN của máy host.

## App Android

```bash
cd android-app
./gradlew assembleDebug -PAPP_API_BASE_URL=http://<IP-LAN>:8000 -PAPP_WS_BASE_URL=ws://<IP-LAN>:8000
```

App Android hiện có các màn hình chat Gemini, đèn và nhạc; các chức năng mới (cảm biến, An ninh, relay, cửa) mới có trên Web.

## Cấu hình firmware

Mọi chân, mức kích và ngưỡng nằm trong [esp32-firmware/include/config.h](esp32-firmware/include/config.h), ví dụ:

- `RELAY_ACTIVE_LOW`, `BUZZER_ACTIVE_LOW`, `LDR_DARK_LEVEL`: mức kích theo module thực tế.
- `MQ2_ALARM_ON` / `MQ2_ALARM_OFF`: ngưỡng báo khói/gas (MQ-2 đang cấp 3V3, chưa hiệu chuẩn).
- `FAN_ON_C` / `FAN_OFF_C`: ngưỡng bật/tắt quạt.
- `DOOR_SENSOR_ENABLED`: bật sau khi đã lắp reed switch; `DOOR_AUTO_CLOSE_MS`: `0` = cửa không tự đóng.

Đổi chân trên mạch thì sửa `config.h`, nạp lại firmware và cập nhật tài liệu đấu nối.

## Giới hạn và an toàn

- Backend **chưa có đăng nhập**: chỉ chạy trong mạng LAN tin cậy, không mở cổng ra Internet. Ai vào được Web trong LAN đều điều khiển được thiết bị và mở cửa.
- Nhận diện khuôn mặt là bản demo: chưa chống giả mạo, **ảnh chụp của thành viên có thể mở được cửa** (và tắt An ninh).
- MQ-2 chỉ cho giá trị tương đối, không phải thiết bị báo cháy/gas đạt chuẩn. Không thử bằng lửa thật hay khí độc; dùng `sim gas on`.
- Mô hình nhận diện, dữ liệu khuôn mặt, nhật ký cửa, `backend/.env` và `esp32-firmware/include/secrets.h` không được commit.
