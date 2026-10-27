import SwiftUI

struct SwitchInfo: Identifiable {
    let id: String
    let name: String
    let symbol: String
    var automation: String?
}

/// Trùng với backend/app/core/rooms.py, esp32-firmware/include/config.h và frontend/src/lib/iot.ts.
enum Catalog {
    static let rooms: [SwitchInfo] = [
        SwitchInfo(id: "phong_khach", name: "Phòng khách", symbol: "lightbulb",
                   automation: "Tự bật khi tối và có chuyển động (An ninh tắt)"),
        SwitchInfo(id: "phong_ngu", name: "Phòng ngủ", symbol: "lightbulb"),
        SwitchInfo(id: "nha_bep", name: "Nhà bếp", symbol: "lightbulb"),
        SwitchInfo(id: "ban_cong", name: "Ban công", symbol: "lightbulb", automation: "Tự bật khi trời tối"),
        SwitchInfo(id: "nha_tam", name: "Nhà tắm", symbol: "lightbulb"),
    ]

    static let relays: [SwitchInfo] = [
        SwitchInfo(id: "relay1", name: "Quạt", symbol: "fan", automation: "Tự bật khi ≥ 33 °C, tắt khi ≤ 31 °C"),
        SwitchInfo(id: "relay2", name: "Đèn phụ", symbol: "lamp.desk", automation: "Tự bật khi có báo trộm"),
    ]

    static let manualOverrideNote = "Lệnh tay giữ ưu tiên 5 phút, sau đó tự động hóa chạy lại."

    static let doorStateLabels: [String: String] = [
        "closed": "Đang đóng",
        "opening": "Đang mở…",
        "open": "Đang mở",
        "closing": "Đang đóng lại…",
        "not_closed": "Chưa đóng hẳn",
    ]

    static func clock(_ epochSeconds: Double) -> String {
        Date(timeIntervalSince1970: epochSeconds).formatted(date: .omitted, time: .standard)
    }
}

extension View {
    /// Hiện lỗi dạng alert; đóng alert thì xóa thông báo.
    func errorAlert(_ message: Binding<String?>) -> some View {
        alert(
            "Không thực hiện được",
            isPresented: Binding(get: { message.wrappedValue != nil }, set: { if !$0 { message.wrappedValue = nil } }),
            actions: { Button("OK", role: .cancel) {} },
            message: { Text(message.wrappedValue ?? "") }
        )
    }
}
