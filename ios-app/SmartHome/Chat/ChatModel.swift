import AVFoundation
import Foundation
import Observation

enum ChatRole {
    case user, assistant, system
}

struct ChatMessage: Identifiable {
    let id: Int
    let role: ChatRole
    var content: String
    var isStreaming = false
    var imageData: Data?
}

/// Trò chuyện với Gemini Live qua `WS /ws/gemini`: văn bản, giọng nói realtime và ảnh.
@MainActor
@Observable
final class ChatModel {
    private static let connectedStatus = "Đã kết nối với Gemini. Bạn có thể trò chuyện ngay!"

    private(set) var messages: [ChatMessage] = []
    private(set) var isConnected = false
    private(set) var statusMessage = "Đang kết nối tới Gemini..."
    private(set) var isListening = false
    var errorMessage: String?

    private var socket: URLSessionWebSocketTask?
    private var nextId = 1
    /// Tin nhắn đang nhận transcription theo từng khúc, mỗi vai một tin.
    private var streamingIds: [ChatRole: Int] = [:]
    /// Tạm tắt micro khi trợ lý nói, bật lại khi trợ lý nói xong.
    private var resumeListeningAfterSpeech = false
    private let audio = GeminiAudio()

    func run(server: Server) async {
        await WebSocketLoop.run(
            url: server.webSocket("/ws/gemini"),
            onOpen: { socket in
                self.socket = socket
                self.isConnected = true
                self.statusMessage = Self.connectedStatus
                self.errorMessage = nil
            },
            onMessage: { self.handle($0) },
            onClose: {
                self.socket = nil
                self.isConnected = false
                self.statusMessage = "Đã ngắt kết nối với Gemini. Đang thử kết nối lại..."
                self.stopListening()
                self.streamingIds = [:]
            }
        )
    }

    // MARK: - Gửi

    func sendText(_ content: String) {
        let text = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        append(.user, text)
        if !send(["text": text]) {
            errorMessage = "Không thể gửi tin nhắn tới Gemini. Đang thử kết nối lại..."
        }
    }

    func sendImage(_ jpeg: Data) {
        guard !jpeg.isEmpty else { return }
        append(.user, "Đã gửi một ảnh", imageData: jpeg)
        if !sendMedia(jpeg, mimeType: "image/jpeg") {
            errorMessage = "Không thể gửi ảnh tới Gemini. Đang thử kết nối lại..."
        }
    }

    func toggleListening() async {
        if isListening {
            stopListening()
            return
        }
        guard isConnected else {
            errorMessage = "Đang kết nối tới Gemini, vui lòng đợi..."
            return
        }
        guard await AVAudioApplication.requestRecordPermission() else {
            errorMessage = "Ứng dụng cần quyền micro để trò chuyện bằng giọng nói. Hãy bật trong Cài đặt của iPhone."
            return
        }
        startListening()
    }

    private func startListening() {
        do {
            try audio.startRecording { [weak self] chunk in
                Task { @MainActor in self?.sendAudio(chunk) }
            }
            isListening = true
            errorMessage = nil
        } catch {
            errorMessage = "Không thể khởi động micro: \(error.localizedDescription)"
        }
    }

    private func stopListening(keepResumeFlag: Bool = false) {
        if !keepResumeFlag {
            resumeListeningAfterSpeech = false
        }
        audio.stopRecording()
        isListening = false
    }

    private func sendAudio(_ chunk: Data) {
        guard isListening else { return }
        if !sendMedia(chunk, mimeType: "audio/pcm;rate=\(GeminiAudio.inputSampleRate)") {
            stopListening()
            errorMessage = "Không thể gửi dữ liệu giọng nói tới Gemini. Đang thử kết nối lại..."
        }
    }

    private func sendMedia(_ data: Data, mimeType: String) -> Bool {
        send(["realtime_input": ["media_chunks": [["mime_type": mimeType, "data": data.base64EncodedString()]]]])
    }

    private func send(_ object: [String: Any]) -> Bool {
        guard let socket,
              let data = try? JSONSerialization.data(withJSONObject: object),
              let text = String(data: data, encoding: .utf8)
        else { return false }
        // Lỗi gửi đồng nghĩa mất kết nối; vòng nhận sẽ phát hiện và kết nối lại.
        socket.send(.string(text)) { _ in }
        return true
    }

    // MARK: - Nhận

    private func handle(_ data: Data) {
        guard let object = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else { return }

        if object["setupComplete"] != nil {
            isConnected = true
            statusMessage = Self.connectedStatus
            return
        }

        switch object["type"] as? String {
        case "keepalive":
            _ = send(["keepalive": ["timestamp": ISO8601DateFormatter().string(from: .now)]])
            return
        case "connection_error":
            let message = object["message"] as? String
                ?? "Không thể kết nối tới Gemini. Ứng dụng đang chuyển sang chế độ ngoại tuyến."
            appendSystem(message)
            statusMessage = message
            return
        case "smart_home_light_update":
            if let location = object["location"] as? String, !location.isEmpty {
                appendSystem("Đèn tại \"\(location)\" hiện đang \(object["is_on"] as? Bool == true ? "bật" : "tắt").")
            }
            return
        case "smart_home_music":
            appendSystem(musicMessage(requested: object["requested_title"] as? String, matched: object["matched_song"] as? String))
            return
        case "smart_home_music_control":
            if let message = musicControlMessage(object) {
                appendSystem(message)
            }
            return
        case "fire_detection_alert":
            onFireAlert(object)
            return
        default:
            break
        }

        if let transcription = object["transcription"] as? [String: Any] {
            let role: ChatRole = transcription["sender"] as? String == "Gemini" ? .assistant : .user
            appendStreaming(
                role,
                text: transcription["text"] as? String ?? "",
                finished: transcription["finished"] as? Bool ?? false
            )
            return
        }

        if let text = object["text"] as? String, !text.isEmpty {
            append(.assistant, text)
            return
        }

        switch object["audio"] {
        case let base64 as String:
            playAssistantAudio(base64: base64, sampleRate: nil)
        case let payload as [String: Any]:
            if let base64 = payload["data"] as? String {
                playAssistantAudio(base64: base64, sampleRate: payload["sample_rate"] as? Int)
            }
        default:
            break
        }
    }

    private func appendStreaming(_ role: ChatRole, text: String, finished: Bool) {
        if role == .assistant, let userId = streamingIds.removeValue(forKey: .user) {
            setStreaming(false, id: userId)
        }
        if !text.isEmpty {
            if let id = streamingIds[role], let index = messages.firstIndex(where: { $0.id == id }) {
                messages[index].content += text
            } else {
                streamingIds[role] = append(role, text, isStreaming: true)
            }
        }
        if finished, let id = streamingIds.removeValue(forKey: role) {
            setStreaming(false, id: id)
        }
        if finished, role == .assistant {
            onAssistantSpeechFinished()
        }
    }

    private func setStreaming(_ isStreaming: Bool, id: Int) {
        if let index = messages.firstIndex(where: { $0.id == id }) {
            messages[index].isStreaming = isStreaming
        }
    }

    private func playAssistantAudio(base64: String, sampleRate: Int?) {
        guard let data = Data(base64Encoded: base64) else { return }
        if isListening {
            resumeListeningAfterSpeech = true
            stopListening(keepResumeFlag: true)
        }
        audio.play(pcm16: data, sampleRate: sampleRate)
    }

    private func onAssistantSpeechFinished() {
        if resumeListeningAfterSpeech && isConnected {
            startListening()
        }
        resumeListeningAfterSpeech = false
    }

    private func musicMessage(requested: String?, matched: String?) -> String {
        if let matched, !matched.isEmpty {
            return "Gemini đang phát bài \"\(matched)\" theo yêu cầu \(requested ?? "")."
        }
        if let requested, !requested.isEmpty {
            return "Không tìm thấy bài hát phù hợp với yêu cầu \"\(requested)\"."
        }
        return "Gemini đang xử lý yêu cầu phát nhạc của bạn."
    }

    private func musicControlMessage(_ object: [String: Any]) -> String? {
        let status = object["status"] as? String
        let matched = object["matched_song"] as? String ?? ""
        switch object["action"] as? String {
        case "pause":
            guard status == "paused" else { return "Không có bài hát nào đang phát để tạm dừng." }
            return matched.isEmpty ? "Đã tạm dừng phát nhạc." : "Đã tạm dừng bài hát \"\(matched)\"."
        case "continue":
            guard status == "playing" else { return "Không có bài hát nào đang tạm dừng để tiếp tục." }
            return matched.isEmpty ? "Tiếp tục phát nhạc." : "Tiếp tục phát bài hát \"\(matched)\"."
        default:
            return nil
        }
    }

    private func onFireAlert(_ object: [String: Any]) {
        var details: [String] = []
        if let detection = object["detection"] as? [String: Any] {
            if let regions = detection["fire_regions"] as? Int, regions > 0 {
                details.append("\(regions) vùng lửa")
            }
            if let confidence = detection["confidence"] as? Double {
                details.append(String(format: "độ tin cậy %.2f%%", confidence))
            }
            if let percentage = detection["fire_percentage"] as? Double {
                details.append(String(format: "ảnh hưởng %.2f%% khung hình", percentage))
            }
        }
        if let triggeredAt = object["triggered_at"] as? String, !triggeredAt.isEmpty {
            details.append("thời gian: \(triggeredAt)")
        }
        let base = (object["message"] as? String).flatMap { $0.isEmpty ? nil : $0 }
            ?? "🚨 Cảnh báo cháy! Vui lòng kiểm tra khu vực ngay lập tức."
        appendSystem(details.isEmpty ? base : "\(base) (\(details.joined(separator: ", ")))")
        if let audio = object["audio_base64"] as? String {
            playAssistantAudio(base64: audio, sampleRate: object["audio_sample_rate"] as? Int)
        }
    }

    // MARK: - Danh sách tin nhắn

    @discardableResult
    private func append(_ role: ChatRole, _ content: String, isStreaming: Bool = false, imageData: Data? = nil) -> Int {
        let id = nextId
        nextId += 1
        messages.append(ChatMessage(id: id, role: role, content: content, isStreaming: isStreaming, imageData: imageData))
        return id
    }

    private func appendSystem(_ message: String) {
        let text = message.trimmingCharacters(in: .whitespacesAndNewlines)
        if !text.isEmpty {
            append(.system, text)
        }
    }
}
