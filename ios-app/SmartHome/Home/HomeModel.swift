import Foundation
import Observation

/// Trạng thái ESP32 qua `WS /ws/iot/updates` và lệnh REST `/iot/*`, `/door/*`.
/// ESP32 là nguồn sự thật: chỉ cập nhật đầu ra từ phản hồi của backend (đã có ACK).
@MainActor
@Observable
final class HomeModel {
    private static let historyLimit = 720

    private(set) var device: IotDeviceInfo?
    private(set) var state: IotDeviceState?
    private(set) var history: [IotHistoryPoint] = []
    private(set) var pending: Set<String> = []
    private(set) var isStreamConnected = false
    private(set) var doorMessage: String?
    var errorMessage: String?

    private var api: APIClient?
    private let decoder = JSONDecoder()

    var isOnline: Bool { device?.online == true && isStreamConnected }

    func run(server: Server) async {
        api = APIClient(server: server)
        device = nil
        state = nil
        history = []
        await WebSocketLoop.run(
            url: server.webSocket("/ws/iot/updates"),
            onOpen: { _ in self.isStreamConnected = true },
            onMessage: { data in
                if let payload = try? self.decoder.decode(IotPayload.self, from: data) {
                    self.apply(payload)
                }
            },
            onClose: { self.isStreamConnected = false }
        )
    }

    func refresh() async {
        guard let api else { return }
        do {
            apply(try await api.get("/iot/state"))
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func setLed(_ roomId: String, on: Bool) {
        send("led:\(roomId)", "/iot/leds/\(roomId)", ["on": on])
    }

    func setRelay(_ relayId: String, on: Bool) {
        send("relay:\(relayId)", "/iot/relays/\(relayId)", ["on": on])
    }

    func setArmed(_ armed: Bool) {
        send("security", "/iot/security", ["armed": armed])
    }

    func silenceAlarm() {
        send("silence", "/iot/alarm/silence")
    }

    func door(open: Bool) {
        guard let api, !pending.contains("door") else { return }
        pending.insert("door")
        doorMessage = nil
        Task {
            defer { pending.remove("door") }
            do {
                let result: DoorResult = try await api.post(open ? "/door/open" : "/door/close")
                doorMessage = !open ? "Đã đóng cửa" : result.disarmed == true ? "Đã mở cửa và tắt chế độ An ninh" : "Đã mở cửa"
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    private func send(_ key: String, _ path: String, _ body: [String: Any] = [:]) {
        guard let api, !pending.contains(key) else { return }
        pending.insert(key)
        Task {
            defer { pending.remove(key) }
            do {
                apply(try await api.post(path, body: body))
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    private func apply(_ payload: IotPayload) {
        device = payload.device
        state = payload.state
        if let snapshot = payload.history {
            history = snapshot
        } else if let point = payload.historyPoint {
            history.append(point)
            if history.count > Self.historyLimit {
                history.removeFirst(history.count - Self.historyLimit)
            }
        }
    }
}
