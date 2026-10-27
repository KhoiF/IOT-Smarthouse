import Foundation
import Observation

/// Điều khiển nhạc phát trên máy chủ (`/smart-home/music/*`); app không tự phát âm thanh.
@MainActor
@Observable
final class MusicModel {
    private(set) var library: [String] = []
    private(set) var playback: MusicPlaybackState?
    private(set) var isBusy = false
    private(set) var infoMessage: String?
    var errorMessage: String?

    /// Thời điểm nhận `playback`, dùng để tự tăng vị trí khi đang phát.
    private var receivedAt = Date()
    private var api: APIClient?
    private let decoder = JSONDecoder()

    func run(server: Server) async {
        api = APIClient(server: server)
        library = []
        playback = nil
        await loadLibrary()
        await WebSocketLoop.run(
            url: server.webSocket("/smart-home/music/updates"),
            onOpen: { _ in },
            onMessage: { data in
                if let state = (try? self.decoder.decode(MusicStreamMessage.self, from: data))?.state {
                    self.apply(state)
                }
            },
            onClose: {}
        )
    }

    func refresh() async {
        await loadLibrary()
        await perform { try await $0.get("/smart-home/music/state") }
    }

    func position(at date: Date) -> Double {
        guard let playback else { return 0 }
        guard playback.status == "playing" else { return playback.positionSeconds }
        let position = playback.positionSeconds + date.timeIntervalSince(receivedAt)
        if let duration = playback.durationSeconds, duration > 0 {
            return min(position, duration)
        }
        return position
    }

    func play(_ title: String) {
        let title = title.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let api, !title.isEmpty else { return }
        isBusy = true
        Task {
            defer { isBusy = false }
            do {
                let response: MusicPlayResponse = try await api.post("/smart-home/music/play", body: ["title": title])
                apply(response.playbackState)
                infoMessage = "Đang phát bài: \(response.selectedSong)"
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }

    func pause() {
        Task { await perform { try await $0.post("/smart-home/music/pause") } }
    }

    func resume() {
        Task { await perform { try await $0.post("/smart-home/music/resume") } }
    }

    func seek(to seconds: Double) {
        Task { await perform { try await $0.post("/smart-home/music/seek", body: ["position_seconds": seconds]) } }
    }

    private func loadLibrary() async {
        guard let api else { return }
        do {
            let songs: [String] = try await api.get("/smart-home/music/library")
            library = songs.sorted()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func perform(_ request: (APIClient) async throws -> MusicPlaybackState) async {
        guard let api else { return }
        do {
            apply(try await request(api))
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    private func apply(_ state: MusicPlaybackState) {
        playback = state
        receivedAt = Date()
        infoMessage = nil
    }
}
