import SwiftUI

struct SettingsView: View {
    let settings: ServerSettings

    @State private var draft = ""
    @State private var testResult: String?
    @State private var isTesting = false

    var body: some View {
        let draftServer = Server(draft)
        NavigationStack {
            Form {
                Section {
                    TextField("192.168.1.10:8000", text: $draft)
                        .keyboardType(.URL)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                    Button("Lưu và kết nối") {
                        if let draftServer {
                            settings.save(draftServer)
                            draft = settings.address
                            testResult = nil
                        }
                    }
                    .disabled(draftServer == nil || draftServer == settings.server)
                } header: {
                    Text("Địa chỉ backend FastAPI")
                } footer: {
                    Text("Dùng IP LAN của máy chạy backend (cổng 8000), iPhone và máy đó phải cùng mạng Wi‑Fi. Trên Simulator có thể dùng localhost:8000.")
                }

                Section {
                    Button(isTesting ? "Đang kiểm tra…" : "Kiểm tra kết nối", action: test)
                        .disabled(settings.server == nil || isTesting)
                    if let testResult {
                        Text(testResult)
                            .font(.footnote)
                    }
                } footer: {
                    if let server = settings.server {
                        Text("Đang dùng \(server.baseURL.absoluteString)")
                    } else {
                        Text("Chưa cấu hình máy chủ.")
                    }
                }
            }
            .navigationTitle("Cài đặt")
            .onAppear {
                if draft.isEmpty {
                    draft = settings.address
                }
            }
        }
    }

    private func test() {
        guard let server = settings.server else { return }
        isTesting = true
        Task {
            defer { isTesting = false }
            do {
                let payload: IotPayload = try await APIClient(server: server).get("/iot/state")
                testResult = "Kết nối máy chủ thành công. ESP32 đang \(payload.device.online ? "online" : "offline")."
            } catch {
                testResult = error.localizedDescription
            }
        }
    }
}
