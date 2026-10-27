import SwiftUI

@main
struct SmartHomeApp: App {
    @State private var settings = ServerSettings()
    @State private var home = HomeModel()
    @State private var chat = ChatModel()
    @State private var music = MusicModel()

    var body: some Scene {
        WindowGroup {
            RootView(settings: settings, home: home, chat: chat, music: music)
        }
    }
}

private enum AppTab {
    case home, chat, music, settings
}

private struct RootView: View {
    let settings: ServerSettings
    let home: HomeModel
    let chat: ChatModel
    let music: MusicModel

    @State private var selection = AppTab.home

    var body: some View {
        TabView(selection: $selection) {
            HomeView(model: home)
                .tabItem { Label("Nhà", systemImage: "house") }
                .tag(AppTab.home)
            ChatView(model: chat)
                .tabItem { Label("Trợ lý AI", systemImage: "waveform") }
                .tag(AppTab.chat)
            MusicView(model: music)
                .tabItem { Label("Nhạc", systemImage: "music.note") }
                .tag(AppTab.music)
            SettingsView(settings: settings)
                .tabItem { Label("Cài đặt", systemImage: "gearshape") }
                .tag(AppTab.settings)
        }
        // Kết nối ở cấp gốc để đổi tab không ngắt WebSocket; đổi địa chỉ máy chủ thì kết nối lại.
        .task(id: settings.server) {
            guard let server = settings.server else {
                selection = .settings
                return
            }
            await withTaskGroup(of: Void.self) { group in
                group.addTask { await home.run(server: server) }
                group.addTask { await chat.run(server: server) }
                group.addTask { await music.run(server: server) }
            }
        }
    }
}
