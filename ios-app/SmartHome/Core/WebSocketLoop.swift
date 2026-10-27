import Foundation

/// Giữ một kết nối WebSocket và tự kết nối lại sau `retryDelay` cho tới khi Task bị hủy.
/// `onOpen` chạy khi nhận message đầu tiên: mọi endpoint của backend đều gửi ngay khi kết nối
/// (`snapshot` hoặc `setupComplete`). Không chờ pong để biết đã mở, vì URLSession chỉ giao pong
/// sau khi các message đến trước đã được `receive()`.
enum WebSocketLoop {
    static func run(
        url: URL,
        retryDelay: Duration = .seconds(3),
        onOpen: @MainActor (URLSessionWebSocketTask) -> Void,
        onMessage: @MainActor (Data) -> Void,
        onClose: @MainActor () -> Void
    ) async {
        while !Task.isCancelled {
            let socket = URLSession.shared.webSocketTask(with: url)
            // Tin nhắn cảnh báo cháy kèm audio base64 có thể lớn hơn mặc định 1 MB.
            socket.maximumMessageSize = 16 * 1024 * 1024
            socket.resume()

            await withTaskCancellationHandler {
                do {
                    let first = try await socket.receive()
                    await onOpen(socket)
                    // Ping định kỳ để URLSession không đóng kết nối khi server im lặng lâu.
                    let keepAlive = Task {
                        while !Task.isCancelled {
                            try await Task.sleep(for: .seconds(20))
                            try await ping(socket)
                        }
                    }
                    defer { keepAlive.cancel() }
                    var message = first
                    while true {
                        switch message {
                        case .string(let text):
                            await onMessage(Data(text.utf8))
                        case .data(let data):
                            await onMessage(data)
                        @unknown default:
                            break
                        }
                        message = try await socket.receive()
                    }
                } catch {
                    // Mất kết nối: rơi xuống phần dọn dẹp và thử lại.
                }
            } onCancel: {
                socket.cancel(with: .goingAway, reason: nil)
            }

            socket.cancel(with: .goingAway, reason: nil)
            await onClose()
            try? await Task.sleep(for: retryDelay)
        }
    }

    private static func ping(_ socket: URLSessionWebSocketTask) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            socket.sendPing { error in
                if let error {
                    continuation.resume(throwing: error)
                } else {
                    continuation.resume()
                }
            }
        }
    }
}
