import Foundation

struct APIError: LocalizedError {
    let message: String

    var errorDescription: String? { message }
}

struct APIClient {
    let server: Server

    private static let decoder = JSONDecoder()

    func get<Response: Decodable>(_ path: String) async throws -> Response {
        try await send(URLRequest(url: server.http(path)))
    }

    func post<Response: Decodable>(_ path: String, body: [String: Any] = [:]) async throws -> Response {
        var request = URLRequest(url: server.http(path))
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: body)
        return try await send(request)
    }

    private func send<Response: Decodable>(_ request: URLRequest) async throws -> Response {
        var request = request
        // Lệnh IoT chờ ESP32 ACK; backend tự trả 504 nếu quá hạn.
        request.timeoutInterval = 15
        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await URLSession.shared.data(for: request)
        } catch {
            throw APIError(message: "Không kết nối được máy chủ: \(error.localizedDescription)")
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard (200..<300).contains(status) else {
            throw APIError(message: Self.detail(from: data) ?? "Máy chủ trả lỗi \(status)")
        }
        do {
            return try Self.decoder.decode(Response.self, from: data)
        } catch {
            throw APIError(message: "Dữ liệu máy chủ trả về không hợp lệ")
        }
    }

    /// FastAPI trả lỗi dạng `{"detail": "..."}` hoặc `{"detail": [{"msg": "..."}]}`.
    private static func detail(from data: Data) -> String? {
        guard let object = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            return nil
        }
        if let text = object["detail"] as? String {
            return text
        }
        if let items = object["detail"] as? [[String: Any]] {
            return items.compactMap { $0["msg"] as? String }.joined(separator: "; ")
        }
        return nil
    }
}
