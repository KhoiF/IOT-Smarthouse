# Hướng dẫn làm việc với project IoT Nhà Thông Minh

Trước khi trả lời câu hỏi kỹ thuật, phân tích hoặc sửa đổi repository này, hãy đọc toàn bộ [`IOT_COMPLETION_GUIDE.md`](IOT_COMPLETION_GUIDE.md). File đó ghi bối cảnh project, phần cứng người dùng có, trạng thái tích hợp thực tế, sơ đồ chân ESP32 và lộ trình hoàn thiện.

Quy tắc làm việc:

- Trả lời người dùng bằng tiếng Việt, trừ khi họ yêu cầu ngôn ngữ khác.
- Phân biệt rõ tính năng đang có trong mã nguồn với nội dung còn là kế hoạch trong cẩm nang. Không khẳng định ESP32, cảm biến hoặc relay đã được tích hợp nếu chưa kiểm tra mã hiện tại.
- Khi bắt đầu một việc mới, đối chiếu cẩm nang với mã nguồn liên quan; nếu project đã thay đổi, ưu tiên sự thật trong mã và cập nhật cẩm nang khi được yêu cầu.
- Giữ ESP32 giao tiếp trực tiếp với FastAPI bằng HTTP và WebSocket; không tự ý chuyển sang MQTT.
- Giữ PlatformIO làm môi trường firmware mặc định.
- Tuân theo phần an toàn điện áp trong cẩm nang. Không hướng dẫn nối tín hiệu 5 V trực tiếp vào GPIO ESP32 hoặc đấu tải điện lưới vào mô hình thử nghiệm.
- Chỉ chạy kiểm tra hoặc test khi người dùng yêu cầu; không commit hoặc công bố thay đổi nếu chưa được yêu cầu.
