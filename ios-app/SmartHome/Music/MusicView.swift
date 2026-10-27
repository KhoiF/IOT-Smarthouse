import SwiftUI

struct MusicView: View {
    @Bindable var model: MusicModel
    @State private var title = ""
    /// Vị trí đang kéo trên thanh tua; nil khi không kéo.
    @State private var scrubPosition: Double?

    var body: some View {
        NavigationStack {
            List {
                Section {
                    nowPlaying
                } header: {
                    Text("Trạng thái phát nhạc")
                } footer: {
                    Text("Âm thanh phát trên máy chủ trung tâm. Ứng dụng đóng vai trò điều khiển từ xa.")
                }

                Section("Phát theo tên") {
                    TextField("Tên bài hát", text: $title)
                        .submitLabel(.go)
                        .onSubmit(playTitle)
                    Button("Phát bài hát", action: playTitle)
                        .disabled(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.isBusy)
                }

                Section("Thư viện nhạc") {
                    if model.library.isEmpty {
                        Text("Chưa có bài hát trong backend/music")
                            .foregroundStyle(.secondary)
                    }
                    ForEach(model.library, id: \.self) { song in
                        Button {
                            model.play(song)
                        } label: {
                            HStack {
                                Label(song, systemImage: "music.note")
                                Spacer()
                                if model.playback?.matchedSong == song, model.playback?.status == "playing" {
                                    Image(systemName: "speaker.wave.2.fill")
                                        .foregroundStyle(.tint)
                                }
                            }
                        }
                        .foregroundStyle(.primary)
                        .disabled(model.isBusy)
                    }
                }
            }
            .navigationTitle("Phát nhạc")
            .refreshable { await model.refresh() }
            .errorAlert($model.errorMessage)
        }
    }

    private var nowPlaying: some View {
        TimelineView(.periodic(from: .now, by: 0.5)) { context in
            let playback = model.playback
            let position = model.position(at: context.date)
            let duration = max(playback?.durationSeconds ?? 0, position + 1)
            VStack(alignment: .leading, spacing: 12) {
                Text(statusText(playback))
                    .fontWeight(.semibold)
                if let info = model.infoMessage {
                    Text(info)
                        .font(.footnote)
                        .foregroundStyle(.tint)
                }
                Slider(
                    value: Binding(get: { scrubPosition ?? position }, set: { scrubPosition = $0 }),
                    in: 0...duration,
                    onEditingChanged: { editing in
                        if !editing, let target = scrubPosition {
                            model.seek(to: target)
                            scrubPosition = nil
                        }
                    }
                )
                .disabled(playback == nil || playback?.status == "stopped")
                HStack {
                    Text(clock(scrubPosition ?? position))
                    Spacer()
                    Text(clock(playback?.durationSeconds ?? 0))
                }
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)
                HStack {
                    Button {
                        model.resume()
                    } label: {
                        Label("Tiếp tục", systemImage: "play.fill").frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(playback?.status != "paused")

                    Button {
                        model.pause()
                    } label: {
                        Label("Tạm dừng", systemImage: "pause.fill").frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.bordered)
                    .disabled(playback?.status != "playing")
                }
            }
            .padding(.vertical, 4)
        }
    }

    private func playTitle() {
        model.play(title)
        title = ""
    }

    private func statusText(_ playback: MusicPlaybackState?) -> String {
        switch playback?.status {
        case "playing": "Đang phát: \(playback?.matchedSong ?? "")"
        case "paused": "Đang tạm dừng: \(playback?.matchedSong ?? "")"
        case nil: "Chưa có trạng thái nhạc"
        default: "Đã dừng phát nhạc"
        }
    }

    private func clock(_ seconds: Double) -> String {
        let total = max(0, Int(seconds))
        return String(format: "%d:%02d", total / 60, total % 60)
    }
}
