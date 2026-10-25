# Cẩm nang hoàn thiện hệ thống IoT Nhà Thông Minh

> Tài liệu này là bối cảnh chuẩn để AI và người phát triển tiếp tục project. Phần “Hiện trạng” mô tả mã nguồn đã có; phần “Lộ trình” là việc cần làm, không phải tính năng đã triển khai.

## 1. Mục tiêu và phạm vi

Hoàn thiện mô hình nhà thông minh dùng ESP32 DevKit V1 làm node thiết bị, backend FastAPI làm máy chủ trung tâm, giao diện web React và ứng dụng Android làm bộ điều khiển. ESP32 đọc cảm biến và điều khiển 5 LED, 2 ngõ relay và buzzer. Backend nhận telemetry, đưa trạng thái tới các ứng dụng và gửi lệnh về ESP32.

Giao tiếp mặc định là HTTP và WebSocket trực tiếp với FastAPI; không thêm MQTT broker. Firmware mới nằm trong thư mục `esp32-firmware/` và được quản lý bằng PlatformIO.

## 2. Hiện trạng project theo mã nguồn

### Đã có

- Backend FastAPI khởi tạo tại `backend/app/main.py`; API đèn và nhạc nằm tại `backend/app/api/routes/smart_home.py`.
- Web React có màn hình điều khiển đèn, nhạc và Gemini trong `frontend/src/`.
- Android app có các màn hình chat, đèn và nhạc trong `android-app/app/src/main/`.
- Trạng thái đèn được ghi vào JSON. Backend phát snapshot và cập nhật qua WebSocket `/smart-home/lights/stream`.
- Thư viện nhạc đọc từ `backend/music`; backend cung cấp file qua HTTP stream. Web phát file bằng audio element.
- Chat Gemini Live dùng `/ws/gemini`. Web có thể gửi camera liên tục từ thiết bị chạy trình duyệt; Android hiện chụp/gửi ảnh trong màn hình chat.
- Nhận diện cháy bằng ảnh dùng OpenCV trong `backend/app/utils/fire_detection.py`. Gemini cần `GOOGLE_API_KEY`; thiếu key thì WebSocket chạy chế độ ngoại tuyến hạn chế.
- Cấu hình ban đầu có 5 vị trí đèn: `Phòng khách`, `Phòng ngủ`, `Nhà bếp`, `Ban công`, `Nhà tắm`.

### Chưa có trong mã hiện tại

- Chưa thấy firmware ESP32 hoặc project PlatformIO.
- Backend hiện lưu trạng thái đèn phần mềm; chưa điều khiển chân GPIO/relay.
- Chưa có API nhận DHT11, MQ-2, PIR, LDR hoặc buzzer alarm từ ESP32.
- Chưa có giao diện hiển thị telemetry và trạng thái online của ESP32.
- Camera giám sát hiện lấy từ trình duyệt; backend chưa tự đọc camera gắn trực tiếp vào ESP32.
- Music service theo dõi trạng thái và stream file; không điều khiển loa vật lý của máy chủ. Màn hình Android hiện điều khiển/trình bày trạng thái, chưa thấy trình phát stream Android.

Không coi các mục trong lộ trình tiếp theo là đã hoàn thành cho tới khi mã nguồn và phần cứng xác nhận.

## 3. Phần cứng người dùng cung cấp

Người dùng cho biết đã mua các linh kiện sau. Phiên bản cụ thể của từng module chưa được xác nhận; trước khi đấu dây cần đọc nhãn, sơ đồ chân và thông số đúng trên module thực tế.

| Linh kiện | SL | Vai trò trong mô hình |
|---|---:|---|
| ESP32 DevKit V1 | 1 | Đọc cảm biến, nhận lệnh qua Wi‑Fi và điều khiển đầu ra |
| Breadboard 400/830 lỗ | 1 | Lắp thử mạch |
| Cáp USB truyền dữ liệu | 1 | Nạp firmware và cấp nguồn ESP32 |
| Dây Dupont đực–đực, đực–cái | 2 bộ | Nối các module |
| LED 5 mm | 5 | Đại diện cho 5 đèn trong phần mềm |
| Điện trở 220–330 Ω | 10 | Mỗi LED dùng một điện trở hạn dòng riêng |
| Buzzer active 5 V hoặc 3.3 V | 1 | Báo động khói/gas cục bộ |
| DHT11 module | 1 | Nhiệt độ và độ ẩm |
| MQ-2 module | 1 | Tín hiệu khói/gas tương đối; cần warm-up và hiệu chuẩn |
| PIR HC-SR501 | 1 | Phát hiện chuyển động |
| Relay 2 kênh, kích được bằng 3.3 V | 1 | Đóng/cắt tối đa hai tải điện áp thấp độc lập |
| Quạt mini hoặc đèn USB 5 V | 1 | Tải thử relay điện áp thấp |
| Adapter 5 V–2 A | 1 | Nguồn ngoài nếu cần cấp module/tải 5 V |
| Module LDR/quang trở | 1 | Đo sáng/tối để phối hợp với PIR |
| Formex/carton, keo, dây rút | 1 bộ | Làm mô hình |

### Quy ước ánh xạ

- 5 LED là 5 đèn phần mềm, ánh xạ theo đúng thứ tự: Phòng khách, Phòng ngủ, Nhà bếp, Ban công, Nhà tắm.
- Relay 1 và relay 2 là hai đầu ra phụ dành cho tải điện áp thấp; chúng không thay thế 5 LED và không đại diện cho đủ 5 phòng.
- PIR được đặt giả định ở khu vực Phòng khách. LDR dùng làm ngưỡng sáng/tối chung cho tự động hóa PIR.
- MQ-2 và camera/OpenCV là hai đường phát hiện khác nhau: MQ-2 đo phản ứng khói/gas của cảm biến, còn OpenCV phân tích ảnh camera. Không gộp chúng thành một phép đo hoặc coi MQ-2 là thiết bị báo cháy đạt chuẩn.

## 4. Sơ đồ chân ESP32 mặc định

Dùng số GPIO in trên bo mạch, không dùng số thứ tự lỗ cắm. Tránh đổi chân nếu chưa cập nhật cả sơ đồ, firmware và cẩm nang.

| Chức năng | Chân ESP32 | Ghi chú |
|---|---:|---|
| LED Phòng khách | GPIO 18 | Nối tiếp điện trở 220–330 Ω |
| LED Phòng ngủ | GPIO 19 | Nối tiếp điện trở 220–330 Ω |
| LED Nhà bếp | GPIO 21 | Nối tiếp điện trở 220–330 Ω |
| LED Ban công | GPIO 22 | Nối tiếp điện trở 220–330 Ω |
| LED Nhà tắm | GPIO 23 | Nối tiếp điện trở 220–330 Ω |
| DHT11 DATA | GPIO 32 | Cấp nguồn theo thông số module; bảo đảm mức DATA không vượt ngưỡng ESP32 |
| PIR OUT | GPIO 33 | Đo mức OUT thực tế trước khi nối |
| MQ-2 AO | GPIO 34 | Chân input-only, ADC1; bắt buộc kiểm tra/chia áp mức analog |
| LDR AO | GPIO 35 | Chân input-only, ADC1; đầu ra phải nằm trong dải an toàn |
| Buzzer control | GPIO 27 | Dùng transistor/MOSFET nếu buzzer cần 5 V hoặc dòng vượt khả năng GPIO |
| Relay IN1 | GPIO 25 | Chỉ dùng module nhận mức điều khiển 3.3 V; xác định active-high/active-low |
| Relay IN2 | GPIO 26 | Chỉ dùng module nhận mức điều khiển 3.3 V; xác định active-high/active-low |

GPIO 34 và 35 chỉ làm input; chúng thuộc ADC1 nên phù hợp để lấy tín hiệu analog khi Wi‑Fi chạy. Xem [tài liệu GPIO Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html). ESP32 không phải thiết bị chịu tín hiệu logic 5 V; giới hạn phải đối chiếu [datasheet ESP32](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf?pubDate=20250709).

## 5. Đấu nối và quy tắc nguồn

Đấu mạch khi đã ngắt USB/adapter. Trước khi cấp nguồn, kiểm tra lại cực tính, chân GPIO và đường GND.

1. **LED:** mỗi GPIO LED → điện trở 220–330 Ω → anode LED (chân dài); cathode (chân ngắn) → GND. Không nối LED trần trực tiếp vào GPIO.
2. **DHT11:** nối VCC, GND, DATA vào nguồn/chân phù hợp với sơ đồ module. Nếu module chạy 5 V, xác minh DATA có kéo lên 5 V hay không; không đưa mức đó vào ESP32 nếu chưa chuyển mức.
3. **PIR HC-SR501:** nối VCC, GND, OUT; GPIO33 đọc OUT. Đợi cảm biến ổn định sau khi cấp nguồn và kiểm tra điện áp OUT.
4. **MQ-2:** module thường cần nguồn 5 V cho heater. Dùng AO cho phép đo analog; đo AO bằng đồng hồ trước. Nếu AO có thể vượt ngưỡng GPIO, lắp chia áp phù hợp (ví dụ hai điện trở 10 kΩ tạo tỉ lệ 1/2 khi đầu vào tối đa 5 V) rồi đo lại trước khi nối GPIO34. Bộ điện trở 220–330 Ω trong danh sách chỉ dùng cho LED, không thay thế bộ chia áp này. Chỉ coi giá trị đọc là mức tương đối sau warm-up/hiệu chuẩn, không quy đổi tùy tiện thành ppm.
5. **LDR:** nối AO tới GPIO35 chỉ sau khi xác nhận điện áp cực đại an toàn. Nếu dùng mạch quang trở tự ráp, dùng bộ chia điện áp với nguồn 3.3 V và GND chung.
6. **Buzzer:** buzzer active phát âm khi được cấp đúng điện áp/cực tính. Với buzzer 5 V hoặc loại tiêu thụ dòng lớn, GPIO27 chỉ điều khiển transistor/MOSFET; không cấp buzzer từ chân GPIO.
7. **Relay:** cấp nguồn cuộn relay theo nhãn module, IN1/IN2 lần lượt tới GPIO25/26. Kiểm tra module thật sự kích được bằng logic 3.3 V; nhiều board relay có logic 5 V hoặc active-low. Tải thử chỉ là quạt/đèn USB 5 V. Không đưa điện lưới 220 V vào mô hình này.
8. **Nguồn:** cấp ESP32 qua USB. Dùng adapter 5 V riêng cho module/tải khi cần và đủ dòng; không nối chập hai nguồn 5 V. Nếu các mạch dùng chung GND, nối GND chung theo sơ đồ. Chỉ duy trì cách ly điện nếu relay/module và sơ đồ cách ly được thiết kế đúng.

**Có thể cần mua thêm:** hai điện trở chia áp có giá trị thích hợp cho AO MQ-2 (nếu module xuất tới 5 V), transistor/MOSFET và điện trở base/gate cho buzzer 5 V, hoặc mạch chuyển mức nếu DHT/PIR/relay đưa ra tín hiệu không tương thích. Không dùng trực tiếp bộ điện trở 220–330 Ω làm chia áp tín hiệu.

## 6. Luồng dữ liệu đích

```text
Web / Android
   │ REST: lệnh đèn, truy vấn trạng thái
   ▼
FastAPI ─── WebSocket cập nhật UI ───► Web / Android
   │
   ├── WebSocket lệnh và ACK ◄──────► ESP32
   └── HTTP telemetry ◄───────────── ESP32
          │
          ├── DHT11: nhiệt độ/độ ẩm
          ├── MQ-2: ADC/cảnh báo gas
          ├── PIR + LDR: chuyển động/ánh sáng
          └── LED/relay/buzzer: trạng thái đầu ra

Camera trình duyệt ──► /ws/gemini ──► Gemini + detector OpenCV hiện có
```

ESP32 kết nối tới địa chỉ LAN của máy chạy backend. Không dùng `localhost` làm host trong firmware: trên ESP32, `localhost` là chính board. Máy tính và ESP32 cần cùng mạng LAN/Wi‑Fi; dùng IP nội bộ của máy chủ và cổng backend `8000`. Android cũng phải đặt URL API/WebSocket phù hợp trong cấu hình build.

### Hợp đồng backend dự kiến

Đây là giao diện cần xây dựng, hiện chưa có trong backend:

- `POST /iot/telemetry`: ESP32 gửi `device_id`, phiên bản firmware, thời gian đo, giá trị DHT11/MQ-2/PIR/LDR và trạng thái LED/relay/buzzer. Bắt đầu với chu kỳ khoảng 2 giây cho số đo định kỳ; có thể gửi ngay khi trạng thái PIR hoặc cảnh báo gas đổi.
- `GET /iot/state`: ứng dụng lấy snapshot telemetry, trạng thái kết nối thiết bị và đầu ra hiện tại.
- `WS /ws/iot/device`: ESP32 đăng ký `device_id`, nhận lệnh đặt đầu ra từ backend và gửi ACK có `command_id` cùng trạng thái thật đã áp dụng.
- `WS /ws/iot/updates`: web/Android nhận snapshot và sự kiện telemetry, kết nối/mất kết nối thiết bị, cảnh báo gas, cập nhật đầu ra.
- Các endpoint `/smart-home/lights/on` và `/smart-home/lights/off` hiện có tiếp tục là giao diện lệnh từ UI/AI. Sau tích hợp, backend phải gửi lệnh tới ESP32, chờ ACK hoặc báo thiết bị offline; UI không được hiển thị lệnh đã thực thi nếu ESP32 chưa xác nhận.

Tên trường JSON, kiểu dữ liệu, mã lỗi và ví dụ hoàn chỉnh phải được ghi đồng bộ ở backend, firmware, web và Android khi triển khai API. Không để client và firmware tự đặt tên khác nhau cho cùng một cảm biến/phòng.

## 7. Quy tắc tự động hóa đã chốt

- **PIR + LDR:** khi LDR đã hiệu chuẩn xác định môi trường tối và PIR tại khu vực Phòng khách phát hiện chuyển động, bật LED Phòng khách. Tắt LED sau 60 giây kể từ lần chuyển động cuối nếu vẫn không có chuyển động và không có lệnh ưu tiên còn hiệu lực.
- **Ưu tiên lệnh người dùng:** lệnh điều khiển từ UI hoặc Gemini giữ quyền ưu tiên trong 5 phút. Trong thời gian đó automation không tự ghi đè đèn được điều khiển; sau đó automation tiếp tục theo cảm biến.
- **MQ-2:** hiệu chuẩn baseline trong môi trường thông thoáng, tránh nguồn khói/gas nguy hiểm. Dùng ngưỡng cảnh báo cấu hình được và hysteresis để tránh bật/tắt còi liên tục. Buzzer phải báo tại ESP32 kể cả khi Wi‑Fi/backend/Gemini mất kết nối; backend và ứng dụng chỉ nhận sự kiện khi mạng còn hoạt động.
- **DHT11/LDR:** gửi số đo định kỳ; hiển thị giá trị và thời điểm cập nhật. Giá trị thô ADC là dữ liệu cảm biến, không phải đơn vị vật lý đã hiệu chuẩn.

## 8. Lộ trình triển khai

### Giai đoạn 1 — Kiểm kê và đấu mạch

- Xác nhận nhãn chân, điện áp nguồn/logic và cực tính trên đúng module đang có.
- Đấu từng khối độc lập theo sơ đồ chân ở trên; đo thử AO MQ-2/LDR và OUT PIR trước khi nối ESP32.
- Thêm linh kiện chia áp/driver còn thiếu trước khi cấp nguồn nếu module yêu cầu.

### Giai đoạn 2 — Firmware ESP32 bằng PlatformIO

- Tạo `esp32-firmware/` với cấu hình PlatformIO cho ESP32 DevKit V1/board tương thích, dùng Arduino framework.
- Tách cấu hình Wi‑Fi, URL backend, `device_id`, map GPIO và ngưỡng cảm biến khỏi logic chính; không commit mật khẩu Wi‑Fi/API key.
- Khởi động đầu ra ở trạng thái an toàn (LED/relay/buzzer tắt), kết nối Wi‑Fi và tự kết nối lại sau mất mạng.
- Đọc DHT11, ADC MQ-2/LDR, PIR; lọc nhiễu và hiệu chuẩn ngưỡng; gửi telemetry HTTP.
- Duy trì WebSocket thiết bị để nhận lệnh output, áp dụng lệnh, gửi ACK. Logic PIR/LDR và buzzer MQ‑2 chạy cục bộ, không phụ thuộc phản hồi server.

### Giai đoạn 3 — Backend FastAPI

- Thêm service ESP32 giữ telemetry mới nhất, thời điểm `last_seen`, kết nối WebSocket và trạng thái đầu ra xác nhận.
- Thêm các endpoint/kênh ở mục 6; kiểm tra dữ liệu vào, báo thiết bị offline và không báo thành công khi không có ACK.
- Kết nối lệnh đèn hiện có với GPIO LED tương ứng; relay phụ có lệnh/hiển thị riêng, không gán relay thành 5 đèn.
- Phát snapshot và cập nhật realtime tới ứng dụng. Không xóa các API đèn/nhạc/Gemini đang dùng.

### Giai đoạn 4 — Giao diện web và Android

- Hiển thị nhiệt độ, độ ẩm, mức MQ-2, trạng thái cảnh báo, ánh sáng, chuyển động, trạng thái ESP32 và thời điểm telemetry gần nhất.
- Cho biết rõ ESP32 đang online/offline và lệnh đầu ra đã được ACK hay chưa.
- Hiển thị/sync trạng thái 5 LED và hai relay; giữ điều khiển đèn bằng REST hiện tại.
- Nhận sự kiện cảnh báo gas và có trạng thái xác nhận/đã đọc ở UI; còi tại ESP32 vẫn tự hoạt động offline.

### Giai đoạn 5 — Hiệu chuẩn, tích hợp và bàn giao

- Kiểm tra lần lượt cảm biến, LED, buzzer và relay; sau đó kiểm tra điều khiển web, Android, Gemini và mất kết nối.
- Ghi vào guide các chân/module thực tế nếu khác sơ đồ mặc định, ngưỡng hiệu chuẩn cuối, địa chỉ LAN, lệnh build/nạp và cách reset/recovery.
- Chỉ đánh dấu mục hoàn tất khi có bằng chứng kiểm tra phần mềm và phần cứng; cập nhật cả `AGENTS.md`/cẩm nang nếu kiến trúc thay đổi.

## 9. Chạy project hiện có và cấu hình mạng

- `start.sh` và `backend/Dockerfile` hiện trỏ Uvicorn tới `app.main:create_app` nhưng không truyền `--factory`; đây là hàm tạo app chứ không phải ASGI app. Trước khi dùng các lệnh này để chạy, cần đổi target thành `app.main:app` (biến app đã khởi tạo trong `main.py`) hoặc truyền tùy chọn `--factory`.
- Sau khi sửa target khởi chạy, cách phát triển là chạy `start.sh` từ thư mục gốc để bật FastAPI và Vite, hoặc chạy hai service trong `docker-compose.yml`.
- Backend mặc định cổng `8000`; web dev server mặc định cổng `5173`.
- ESP32 cần IP LAN của máy chạy backend, không dùng `localhost`. Nếu chạy trong Docker, bảo đảm port `8000` được expose và biến môi trường/API key cần thiết được truyền vào container.
- Cấu hình `GOOGLE_API_KEY` nếu cần Gemini Live/TTS. Không ghi khóa Gemini hoặc mật khẩu Wi‑Fi vào tài liệu, commit hoặc log.
- Chỉ mở các cổng trong mạng tin cậy khi demo; backend hiện chưa có lớp xác thực người dùng cho các API điều khiển. Không port-forward backend ra Internet khi chưa bổ sung xác thực.

## 10. Ma trận kiểm tra và tiêu chí hoàn tất

| Kiểm tra | Thao tác | Kết quả đạt |
|---|---|---|
| Firmware và Wi‑Fi | Khởi động ESP32, ngắt rồi khôi phục Wi‑Fi | ESP32 kết nối lại, backend cập nhật online/last_seen |
| 5 LED | Bật/tắt từng vị trí từ web rồi Android | Chỉ LED đúng phòng đổi; ACK và UI khớp trạng thái thật |
| Relay | Đóng/cắt quạt/đèn USB 5 V thử nghiệm | Tải đổi trạng thái đúng; ESP32 không reset khi relay đóng/cắt |
| DHT11 | Theo dõi vài chu kỳ và so sánh nhiệt độ/độ ẩm môi trường | Số đo mới lên backend/UI, lỗi sensor không làm treo vòng lặp |
| MQ-2 | Chờ warm-up, ghi baseline và thử theo quy trình an toàn của module | ADC ổn định tương đối; vượt ngưỡng tạo cảnh báo và buzzer cục bộ |
| PIR/LDR | Thử sáng/tối, có/không chuyển động | Chỉ bật Phòng khách khi tối + có chuyển động; auto-off sau 60 giây |
| Ưu tiên lệnh tay/AI | Gửi lệnh trong lúc automation hoạt động | Đèn giữ lệnh ưu tiên 5 phút, sau đó automation tiếp tục |
| Mất Internet/backend | Ngắt mạng khi gây điều kiện PIR hoặc MQ-2 | Logic tại ESP32 tiếp tục; reconnect và đồng bộ lại sau khi mạng phục hồi |
| Gemini/OpenCV | Bật camera web có Gemini key, gửi ảnh thử đã chọn | Chat/ảnh hoạt động; cảnh báo OpenCV nếu detector phát hiện theo ngưỡng |

Không dùng lửa thật, khí độc hoặc tải điện lưới để thử nghiệm mô hình. MQ-2 là cảm biến demo, không thay thế thiết bị báo cháy/gas đạt chuẩn an toàn.

## 11. Cách AI nên sử dụng cẩm nang

Khi nhận yêu cầu về repository này:

1. Đọc file này trước để biết kiến trúc, phần cứng, pin map và quy tắc đã chốt.
2. Kiểm tra mã nguồn hiện tại của subsystem người dùng hỏi; mã nguồn mới hơn tài liệu thì mã nguồn là sự thật về trạng thái hiện tại.
3. Nói rõ phần nào đã được triển khai, phần nào còn là lộ trình. Không giả định firmware/API mới đã tồn tại chỉ vì được mô tả ở đây.
4. Trước khi sửa wiring/pin/protocol/automation, cập nhật đồng bộ các lớp bị ảnh hưởng và tài liệu; giữ an toàn điện áp ESP32.
5. Khi cần thay đổi pin hoặc giao thức đã chốt, nêu tác động tới firmware, backend, web/Android và cẩm nang.
