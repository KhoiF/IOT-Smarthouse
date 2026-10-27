import SwiftUI

struct ChatView: View {
    @Bindable var model: ChatModel
    @State private var input = ""
    @State private var isCameraPresented = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                statusBar
                messageList
                if let error = model.errorMessage {
                    Text(error)
                        .font(.footnote)
                        .foregroundStyle(.red)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.horizontal)
                        .padding(.top, 6)
                }
                inputBar
            }
            .navigationTitle("Trợ lý AI")
            .navigationBarTitleDisplayMode(.inline)
            .fullScreenCover(isPresented: $isCameraPresented) {
                CameraPicker { model.sendImage($0) }
                    .ignoresSafeArea()
            }
        }
    }

    private var statusBar: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(model.isConnected ? .green : .orange)
                .frame(width: 10, height: 10)
            Text(model.statusMessage)
                .font(.footnote)
                .foregroundStyle(.secondary)
            Spacer()
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(.bar)
    }

    private var messageList: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(spacing: 12) {
                    ForEach(model.messages) { message in
                        MessageBubble(message: message)
                            .id(message.id)
                    }
                }
                .padding()
            }
            .overlay {
                if model.messages.isEmpty {
                    ContentUnavailableView(
                        "Hãy bắt đầu cuộc trò chuyện với trợ lý AI.",
                        systemImage: "bubble.left.and.bubble.right",
                        description: Text("Nhập tin nhắn, nhấn micro để nói hoặc chụp ảnh gửi Gemini.")
                    )
                }
            }
            .scrollDismissesKeyboard(.interactively)
            .onChange(of: model.messages.last?.content) {
                if let last = model.messages.last {
                    withAnimation { proxy.scrollTo(last.id, anchor: .bottom) }
                }
            }
        }
    }

    private var inputBar: some View {
        HStack(spacing: 10) {
            Button {
                Task { await model.toggleListening() }
            } label: {
                Image(systemName: model.isListening ? "mic.fill" : "mic.slash")
                    .font(.title3)
                    .frame(width: 44, height: 44)
                    .background(model.isListening ? Color.red.opacity(0.2) : Color(.secondarySystemBackground), in: Circle())
            }
            .accessibilityLabel(model.isListening ? "Dừng nghe" : "Nhấn để nói")
            .disabled(!model.isConnected)

            TextField("Nhập tin nhắn", text: $input)
                .textFieldStyle(.roundedBorder)
                .submitLabel(.send)
                .onSubmit(send)
                .disabled(!model.isConnected)

            Button {
                isCameraPresented = true
            } label: {
                Image(systemName: "camera")
                    .font(.title3)
            }
            .accessibilityLabel("Chụp ảnh gửi Gemini")
            .disabled(!model.isConnected)

            Button(action: send) {
                Image(systemName: "paperplane.fill")
                    .font(.title3)
            }
            .accessibilityLabel("Gửi")
            .disabled(!model.isConnected || input.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        }
        .padding()
    }

    private func send() {
        model.sendText(input)
        input = ""
    }
}

private struct MessageBubble: View {
    let message: ChatMessage

    var body: some View {
        HStack {
            if message.role != .assistant { Spacer(minLength: 40) }
            VStack(alignment: .leading, spacing: 8) {
                if let data = message.imageData, let image = UIImage(data: data) {
                    Image(uiImage: image)
                        .resizable()
                        .scaledToFit()
                        .frame(maxHeight: 240)
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                }
                if !message.content.isEmpty {
                    Text(message.content)
                        .italic(message.isStreaming)
                        .multilineTextAlignment(message.role == .system ? .center : .leading)
                        .textSelection(.enabled)
                }
            }
            .padding(12)
            .background(background, in: RoundedRectangle(cornerRadius: 16))
            .font(message.role == .system ? .footnote : .body)
            if message.role != .user { Spacer(minLength: 40) }
        }
    }

    private var background: Color {
        switch message.role {
        case .user: .accentColor.opacity(0.2)
        case .assistant: Color(.secondarySystemBackground)
        case .system: .orange.opacity(0.15)
        }
    }
}
