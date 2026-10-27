import Foundation
import Observation

/// Địa chỉ backend FastAPI, ví dụ `http://192.168.1.10:8000`. WebSocket dùng cùng host (ws/wss).
struct Server: Equatable {
    let baseURL: URL

    /// Nhận `192.168.1.10:8000` hoặc `http://192.168.1.10:8000/`; thiếu scheme thì mặc định http.
    init?(_ text: String) {
        var value = text.trimmingCharacters(in: .whitespacesAndNewlines)
        while value.hasSuffix("/") {
            value.removeLast()
        }
        if !value.contains("://") {
            value = "http://" + value
        }
        guard let url = URL(string: value),
              let scheme = url.scheme?.lowercased(), scheme == "http" || scheme == "https",
              let host = url.host, !host.isEmpty
        else { return nil }
        baseURL = url
    }

    func http(_ path: String) -> URL {
        URL(string: baseURL.absoluteString + path)!
    }

    func webSocket(_ path: String) -> URL {
        var components = URLComponents(url: http(path), resolvingAgainstBaseURL: false)!
        components.scheme = components.scheme == "https" ? "wss" : "ws"
        return components.url!
    }
}

/// Lưu địa chỉ backend trong UserDefaults để đổi IP LAN mà không phải build lại app.
@Observable
final class ServerSettings {
    private static let storageKey = "serverURL"

    private(set) var address: String

    var server: Server? { Server(address) }

    init() {
        address = UserDefaults.standard.string(forKey: Self.storageKey) ?? ""
    }

    func save(_ server: Server) {
        address = server.baseURL.absoluteString
        UserDefaults.standard.set(address, forKey: Self.storageKey)
    }
}
