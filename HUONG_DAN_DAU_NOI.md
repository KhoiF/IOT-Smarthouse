# Hướng dẫn đấu nối cảm biến và thiết bị với ESP32

Tài liệu này mô tả cách đấu **đúng với mạch nhóm đang dùng**, đã thử trên phần cứng thật. Một số điểm khác với sơ đồ dự kiến trong [IOT_COMPLETION_GUIDE.md](IOT_COMPLETION_GUIDE.md) (LDR, MQ-2, còi). Khi hai tài liệu khác nhau, làm theo file này.

Chân và ngưỡng trong firmware nằm ở [esp32-firmware/include/config.h](esp32-firmware/include/config.h). Nếu đổi chân trên mạch, phải sửa file đó rồi nạp lại.

## 1. Quy tắc an toàn

- **Rút USB và adapter trước khi cắm hoặc rút bất kỳ dây nào.**
- ESP32 chỉ chịu tín hiệu **3,3 V**. Không đưa mức 5 V vào chân GPIO.
- Chỉ dùng tải điện áp thấp (quạt mini, đèn 3,3–5 V). **Không đưa điện lưới 220 V vào mô hình.**
- Không thử MQ-2 bằng lửa, bật lửa gas hay khí độc. Dùng lệnh giả lập `sim gas on` (mục 6).
- Đọc **số GPIO in trên bo** (ví dụ `D18`), không đếm theo thứ tự lỗ cắm.

## 2. Bảng chân tổng hợp

| Thiết bị | Chân thiết bị | Nối tới ESP32 | Nguồn |
|---|---|---|---|
| LED Phòng khách | anode qua điện trở | GPIO 18 | – |
| LED Phòng ngủ | anode qua điện trở | GPIO 19 | – |
| LED Nhà bếp | anode qua điện trở | GPIO 21 | – |
| LED Ban công | anode qua điện trở | GPIO 22 | – |
| LED Nhà tắm | anode qua điện trở | GPIO 23 | – |
| DHT11 | DATA | GPIO 32 | 3V3 |
| PIR HC-SR501 | OUT (chân giữa) | GPIO 33 | 5V (VIN) |
| MQ-2 | AO | GPIO 34 | 3V3 (tạm thời, xem mục 4.4) |
| LDR (module 3 chân) | DO | GPIO 35 | 3V3 |
| Còi (module 3 chân) | I/O | GPIO 27 | 3V3 |
| Relay 2 kênh | IN1 / IN2 | GPIO 25 / GPIO 26 | 5V (VIN) |
| Servo SG90 (cửa) | dây tín hiệu (cam/vàng) | GPIO 13 | 5V (VIN) + tụ 470 µF, hoặc adapter 5 V riêng (mục 4.8) |
| Reed switch (cửa) | một chân | GPIO 16 (chân kia nối GND) | – |

Mọi chân `GND` của module đều nối về **ray GND chung**.

## 3. Chuẩn bị breadboard

Breadboard nối sẵn theo quy tắc:
- Mỗi **hàng** có 5 lỗ `a–e` thông nhau, và 5 lỗ `f–j` thông nhau. Hai nửa bị rãnh giữa ngăn cách.
- Mỗi **ray dọc** ở mép (`+` đỏ, `−` xanh) thông suốt chiều dài.
- Hai linh kiện cắm cùng một hàng (cùng nửa) là đã nối với nhau. Một lỗ chỉ cắm được một chân.

Dùng ba đường nguồn chung:

| Ray | Nối từ ESP32 | Dùng cho |
|---|---|---|
| Ray `−` | chân `GND` | GND của mọi module |
| Ray `+` phía thứ nhất | chân `3V3` | DHT11, LDR, MQ-2, còi |
| Ray `+` phía thứ hai | chân `VIN` (5 V khi cắm USB) | PIR, relay |

- Dán nhãn "3V3" và "5V" lên hai ray `+`. **Hai ray này không được chạm nhau.**
- Chỉ nối **một** dây GND từ ESP32 vào ray `−`. Không cắm GND vào ray `+`.
- Nếu breadboard chỉ có một cặp ray, cấp 5 V cho PIR và relay bằng dây Dupont nối thẳng từ `VIN` tới chân VCC của từng module.

## 4. Đấu từng thiết bị

Lắp và thử **từng khối một** (mục 6), xong khối này mới sang khối khác.

### 4.1. 5 LED

```
GPIO ──dây── [hàng A] ──điện trở 220–330 Ω── [hàng B] = anode LED (chân dài)
                                             [hàng C] = cathode LED (chân ngắn) ──dây── ray GND
```

Ví dụ với LED Phòng khách:
1. Điện trở: một chân vào `a8`, chân kia vào `a10`.
2. LED: chân dài vào `e10`, chân ngắn vào `e11`.
3. Dây từ **D18** vào `b8`.
4. Dây từ `a11` sang ray `−`.

Bốn LED còn lại làm giống hệt, mỗi LED chiếm 3 hàng riêng:

| LED | Chân | Hàng GPIO + điện trở | Hàng anode | Hàng cathode |
|---|---|---|---|---|
| Phòng khách | D18 | 8 | 10 | 11 |
| Phòng ngủ | D19 | 14 | 16 | 17 |
| Nhà bếp | D21 | 20 | 22 | 23 |
| Ban công | D22 | 26 | 28 | 29 |
| Nhà tắm | D23 | 32 | 34 | 35 |

Mỗi LED phải có **điện trở riêng**. LED cắm ngược chiều thì không sáng, chỉ cần đảo lại.

### 4.2. DHT11 (nhiệt độ, độ ẩm)

| Chân module | Nối tới |
|---|---|
| `VCC` / `+` | ray 3V3 |
| `GND` / `-` | ray GND |
| `DATA` / `OUT` / `S` | GPIO 32 |

Module 3 chân đã có điện trở kéo lên, không cần thêm. Thứ tự chân khác nhau giữa các loại, nên **đọc nhãn trên module**.

### 4.3. LDR – cảm biến ánh sáng (module 3 chân VCC, GND, DO)

| Chân module | Nối tới |
|---|---|
| `VCC` | ray 3V3 |
| `GND` (ký hiệu ⏚) | ray GND |
| `DO` | GPIO 35 |

- Module nhóm đang dùng (chip LM393) **chỉ có DO, không có AO**, nên chỉ cho biết **Tối/Sáng**, không đo được cường độ sáng.
- Đã đo trên mạch: **tối thì DO = HIGH (1)**. Trong firmware là `LDR_DARK_LEVEL = HIGH`.
- **Chỉnh biến trở xanh** trên module sao cho ở ánh sáng phòng bình thường thì Serial báo `sang`, còn che tay thì báo `TOI`.
- Không cấp 5 V cho module này khi DO đang nối vào GPIO.

### 4.4. MQ-2 – khói/gas

**Cách đang dùng (tạm thời):**

| Chân module | Nối tới |
|---|---|
| `VCC` | ray 3V3 |
| `GND` | ray GND |
| `AO` | GPIO 34 (nối thẳng) |
| `DO` | không nối |

Cấp 3,3 V thì AO không thể vượt 3,3 V, nên nối thẳng được. Đổi lại, cuộn nung chạy thiếu điện áp, cảm biến kém nhạy, và ngưỡng báo cháy hiện tại (`MQ2_ALARM_ON = 3600`) chưa được hiệu chuẩn.

**Cách đúng (khi có 2 điện trở 10 kΩ):**

```
MQ-2 AO ──[10 kΩ]──┬── GPIO 34
                   │
                [10 kΩ]
                   │
                  GND
```

- `VCC` của MQ-2 nối **5V (VIN)**.
- **Đo điểm giữa bộ chia bằng đồng hồ** trước khi cắm vào GPIO 34; phải dưới 3,3 V.
- **Không dùng điện trở 220–330 Ω** của LED để làm bộ chia này.
- Sau khi chuyển sang 5 V, đo lại giá trị nền rồi sửa `MQ2_ALARM_ON` / `MQ2_ALARM_OFF` trong `config.h`.

MQ-2 cần khoảng **2 phút warm-up** và nóng tay khi chạy; đó là bình thường. Firmware chưa xét cảnh báo gas trong 2 phút đầu.

### 4.5. PIR HC-SR501 – chuyển động

| Chân | Nối tới |
|---|---|
| VCC (chân ngoài) | 5V (VIN) |
| OUT (**chân giữa**) | GPIO 33 |
| GND (chân ngoài còn lại) | ray GND |

Module nhóm đang dùng (mã `HW-416-B`) **không in nhãn chân**. Chân giữa luôn là OUT. Với hai chân ngoài, xác định GND trước khi cấp nguồn, vì đảo VCC và GND có thể làm hỏng module:
- **Bằng đồng hồ (chế độ thông mạch):** một que đặt vào chân âm (−) của tụ hóa trên module, que kia chạm lần lượt hai chân ngoài. Chân kêu bíp là GND.
- **Không có đồng hồ:** nối GND của ESP32 vào chân âm của tụ hóa, đặt GPIO33 ở `INPUT_PULLUP`, rồi chạm lần lượt hai chân ngoài. Chân làm giá trị đọc về `0` là GND. PIR phải **chưa cấp nguồn** khi làm cách này.

Sau khi cấp nguồn:
- Đợi **30–60 giây** để PIR ổn định. Firmware bỏ qua PIR trong 60 giây đầu.
- Hai biến trở cam ở mặt có chip `BISS0001`: một cái chỉnh độ nhạy, một cái chỉnh thời gian giữ. **Vặn thời gian giữ về mức thấp nhất**, nếu không OUT có thể giữ mức 1 tới vài phút, và còi báo trộm cũng kêu lâu theo.
- Nên đo mức OUT bằng đồng hồ một lần. Mức đúng là khoảng 3,3 V khi có chuyển động.

### 4.6. Còi (module 3 chân VCC, GND, I/O)

| Chân module | Nối tới |
|---|---|
| `VCC` | ray **3V3** |
| `GND` | ray GND |
| `I/O` | GPIO 27 |

- Module này **kích mức thấp**: I/O = LOW thì kêu, HIGH thì im (đã đo). Trong firmware là `BUZZER_ACTIVE_LOW = true`.
- **Không cấp 5 V cho module.** Khi VCC = 5 V, mức HIGH 3,3 V của ESP32 không tắt được transistor bên trong, nên còi kêu liên tục.
- Có thể nghe một tiếng "tít" ngắn khi bấm EN hoặc lúc nạp code, vì trong vài mili giây đó firmware chưa chạy. Muốn bỏ hẳn, thêm một điện trở 10 kΩ nối I/O lên 3V3.

### 4.7. Relay 2 kênh

**Phía điều khiển:**

| Chân module | Nối tới |
|---|---|
| `VCC` | 5V (VIN) |
| `GND` | ray GND |
| `IN1` | GPIO 25 |
| `IN2` | GPIO 26 |

Giữ nguyên jumper `JD-VCC` nếu module có.

**Phía tải** (mỗi kênh có 3 cọc vít `NO`, `COM`, `NC`; dùng `COM` và `NO`):

```
Relay 1 – quạt 5 V:   5V (+) ──► COM1    NO1 ──► quạt (+)    quạt (−) ──► GND
Relay 2 – đèn 3,3 V:  3V3   ──► COM2    NO2 ──► đèn (+)     đèn (−)  ──► GND
```

- Mỗi kênh chỉ đóng/cắt **một** mạch. Đèn 3,3 V và quạt 5 V phải ở hai kênh riêng. Không nối nhầm đèn 3,3 V vào kênh có nguồn 5 V.
- Nếu ESP32 bị reset khi quạt khởi động, cấp quạt bằng **adapter 5 V riêng**, nối **GND adapter với GND ESP32**, và **không nối chung hai đường 5 V** khi USB đang cắm.
- Firmware đang giả định relay **kích mức thấp** (`RELAY_ACTIVE_LOW = true`). Mức này **chưa được xác nhận** trên module của nhóm. Nếu vừa cấp nguồn mà relay đã đóng (có tiếng tách, đèn báo trên module sáng, quạt chạy), đổi thành `false` rồi nạp lại.
- Khi có cảnh báo cháy, firmware tự ngắt cả hai relay để mô phỏng "ngắt cầu dao".

### 4.8. Cửa: servo SG90 và công tắc từ (reed switch)

**Servo SG90** (3 dây): chọn **một** trong hai cách cấp nguồn.

**Cách A – nối vào 5V (VIN) của ESP32** (gọn nhất, đủ cho mô hình khi cấp nguồn qua USB):

| Dây servo | Nối tới |
|---|---|
| Nâu/đen (GND) | ray GND |
| Đỏ (VCC) | ray **5V (VIN)**, chung ray với PIR và relay |
| Cam/vàng (tín hiệu) | GPIO 13 |

```
ESP32 VIN ──┬──────────────► servo đỏ (VCC)
            │
          [tụ 470–1000 µF]   (chân dài/+ về phía VIN, chân −/vạch trắng về GND)
            │
ESP32 GND ──┴──────────────► servo nâu (GND)
ESP32 GPIO13 ──────────────► servo cam (tín hiệu)
```

- Chân VIN lấy 5 V từ cổng USB (qua diode trên board nên thường còn khoảng 4,6–4,9 V; servo SG90 vẫn chạy tốt).
- Servo kéo dòng lớn lúc bắt đầu quay (có thể vài trăm mA, lúc kẹt tới khoảng 0,7 A), cộng với relay và PIR trên cùng đường 5V. **Bắt buộc cắm tụ 470–1000 µF** sát servo để giảm sụt áp.
- Firmware chỉ phát xung khoảng 0,7 giây mỗi lần đóng/mở rồi ngắt, nên lúc đứng yên servo gần như không tốn điện.
- Không để cánh cửa bị kẹt hoặc vướng: servo kẹt sẽ kéo dòng tối đa liên tục.
- Dùng cáp USB tốt, cắm thẳng vào máy (không qua hub).
- **Dấu hiệu nguồn không đủ:** ESP32 tự khởi động lại mỗi khi cửa mở/đóng, Web báo ESP32 mất kết nối, hoặc log backend ghi `lý do khởi động gần nhất: brownout`. Khi đó chuyển sang **cách B**.

**Cách B – adapter 5 V riêng** (ổn định nhất, nên dùng khi có cả quạt trên relay):

| Dây servo | Nối tới |
|---|---|
| Nâu/đen (GND) | GND của adapter **và** ray GND của ESP32 (GND chung) |
| Đỏ (VCC) | 5V của adapter |
| Cam/vàng (tín hiệu) | GPIO 13 |

- **Không nối** 5V của adapter vào chân VIN của ESP32 khi đang cắm USB (hai nguồn 5 V chập nhau).
- Vẫn cắm tụ 470–1000 µF giữa 5V và GND sát servo.
- Nếu quạt (relay 1) cũng chạy bằng adapter, có thể cấp chung adapter 5 V–2 A cho cả quạt và servo.

**Sau khi nối (cả hai cách):**
- Gắn cánh tay servo vào cánh cửa mô hình. Firmware dùng 0° là **đóng**, 90° là **mở** (`DOOR_CLOSED_ANGLE` / `DOOR_OPEN_ANGLE` trong `config.h`); đổi nếu cửa mở ngược chiều.
- Firmware tự đưa servo về vị trí đóng khi khởi động, nên lắp cánh tay servo **sau** khi đã cấp nguồn một lần.

**Reed switch** (2 chân, không phân cực):
- Một chân nối **GPIO 16**, chân kia nối **GND**. Không cần điện trở (firmware bật điện trở kéo lên bên trong).
- Gắn reed switch lên khung cửa, nam châm lên cánh cửa: cửa đóng thì nam châm áp sát.
- Lắp xong, đổi `DOOR_SENSOR_ENABLED = true` trong `config.h` rồi nạp lại. Khi chưa lắp mà bật cờ này, chân bỏ trống đọc là "cửa hở" và gây báo trộm giả.

Cách hoạt động:
- Lệnh mở (nhận diện khuôn mặt hoặc nút "Mở cửa"): servo quay 90°, giữ **5 giây**, rồi tự đóng, kể cả khi mất mạng.
- **Cảnh báo cháy**: cửa mở và giữ mở để thoát hiểm; hết cháy 5 giây thì tự đóng.
- **An ninh bật** mà reed switch báo cửa hở trong khi firmware không ra lệnh mở → **báo trộm** (còi + đèn phụ). Mở cửa bằng nhận diện không tắt An ninh, nên PIR vẫn báo nếu có người đi vào.

## 5. Kiểm tra trước khi cắm USB

- [ ] Chỉ có một dây GND từ ESP32 vào ray `−`, và không dây GND nào cắm vào ray `+`.
- [ ] Ray 3V3 và ray 5V không chạm nhau.
- [ ] DHT11, LDR, MQ-2 (cách tạm thời) và còi lấy nguồn từ **3V3**.
- [ ] PIR, relay (và servo nếu dùng cách A) lấy nguồn từ **5V (VIN)**; servo có tụ 470–1000 µF đúng cực.
- [ ] Mỗi LED có điện trở riêng, chân dài về phía điện trở.
- [ ] Không có chân 5 V nào nối thẳng vào GPIO.
- [ ] Tải của relay chỉ là thiết bị 3,3–5 V.

## 6. Thử từng khối bằng Serial Monitor

Firmware có sẵn lệnh thử, không cần mạng.

1. Nạp firmware: mở thư mục `esp32-firmware` trong VS Code (extension PlatformIO), bấm **Upload (→)**.
2. Mở Serial Monitor: bấm biểu tượng **phích cắm 🔌** ở thanh dưới cùng (tốc độ 115200 đã được cấu hình sẵn).
3. Gõ lệnh rồi nhấn Enter. Monitor không hiện chữ đang gõ, đó là bình thường. Gõ `log` để tắt dòng log 2 giây/lần cho dễ đọc.

| Khối | Lệnh / thao tác | Kết quả đúng |
|---|---|---|
| Tổng quát | `status` | In đủ cảm biến, 5 LED, 2 relay, An ninh, cảnh báo |
| LED | `led 1 on` … `led 5 off` | Đúng LED của phòng đó bật/tắt |
| DHT11 | Hà hơi vào cảm biến, xem `status` | Độ ẩm tăng rõ, nhiệt độ tăng nhẹ |
| LDR | Che tay ~3 giây | `Anh sang: TOI`, LED Ban công tự bật |
| PIR | Sau 60 giây, vẫy tay | `Chuyen dong: CO` |
| MQ-2 | `sim gas on` | Còi kêu dồn dập, relay bị ngắt, `relay 1 on` báo lỗi |
| Còi | `silence`, rồi `sim gas off` | Còi im, rồi hết cảnh báo |
| Relay | `relay 1 on` / `relay 2 on` | Có tiếng tách, quạt/đèn chạy đúng kênh |
| An ninh | `arm`, đợi 2 giây, vẫy tay | Còi báo trộm (chậm), đèn phụ bật; hết chuyển động ~2 giây thì cả hai tự tắt |
| | `disarm` | Tắt chế độ An ninh |
| Cửa | `door open` | Servo quay sang mở, 5 giây sau tự đóng; `status` hiện trạng thái cửa |

Dòng log định kỳ có dạng:

```
T=30.4 H=60 MQ2=154* toi=0 cd=1 | LED=10000 R=00 coi=0 | an_ninh=0 chay=0 trom=0
```

`T` nhiệt độ, `H` độ ẩm, `MQ2` giá trị ADC (`*` = đang warm-up), `toi` 1 = tối, `cd` 1 = có chuyển động, `LED` theo thứ tự Phòng khách → Nhà tắm, `R` relay 1 và 2, `coi` còi đang kêu.

Muốn điều khiển từ Web, xem phần cấu hình Wi‑Fi trong `esp32-firmware/include/secrets.example.h` (sao chép thành `secrets.h`, điền Wi‑Fi 2,4 GHz và IP LAN của máy chạy backend).

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| Không thấy cổng ESP32 khi nạp | Cáp chỉ sạc, không truyền dữ liệu | Đổi cáp USB khác |
| Nạp lỗi `No more data to read` | Tốc độ nạp quá cao | Đã đặt sẵn 115200; nếu vẫn lỗi, giữ nút BOOT khi bấm Upload |
| Nạp lỗi `port is busy` | Serial Monitor đang mở | Đóng Serial Monitor rồi nạp lại |
| LED không sáng | Cắm ngược LED, điện trở và LED khác hàng, chưa nối GND | Kiểm tra lại theo mục 4.1 |
| DHT11 báo `loi doc` liên tục | Sai chân DATA hoặc thiếu nguồn | Kiểm tra DATA → D32, VCC → 3V3 |
| LDR luôn báo `TOI` hoặc luôn `sang` | Ngưỡng biến trở chưa đúng | Xoay biến trở xanh trên module |
| PIR luôn báo có chuyển động | Đang trong 60 giây khởi động, hoặc thời gian giữ quá dài | Đợi, rồi vặn biến trở thời gian giữ về thấp nhất |
| Còi kêu liên tục dù không có cảnh báo | Module còi cấp 5 V, hoặc I/O cắm sai chân | VCC về 3V3, I/O về D27; thử `buzz high` (phải im) / `buzz low` (phải kêu) |
| Relay đóng ngay khi cấp nguồn | Relay kích mức cao | Đổi `RELAY_ACTIVE_LOW = false` trong `config.h` |
| ESP32 reset khi quạt chạy | Sụt áp do tải | Cấp quạt bằng adapter riêng, nối GND chung |
