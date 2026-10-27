import AVFoundation

/// Ghi micro thành PCM 16 kHz mono 16-bit (định dạng `/ws/gemini` nhận) và phát PCM 16-bit Gemini trả về.
final class GeminiAudio {
    static let inputSampleRate = 16_000
    private static let defaultOutputSampleRate = 24_000

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private var playerFormat = AVAudioFormat(
        commonFormat: .pcmFormatFloat32, sampleRate: Double(defaultOutputSampleRate), channels: 1, interleaved: false
    )!
    private var isConfigured = false
    private var isVoiceProcessingEnabled = false
    private(set) var isRecording = false

    /// `onChunk` chạy trên luồng audio, mỗi lần khoảng 100 ms.
    func startRecording(onChunk: @escaping (Data) -> Void) throws {
        guard !isRecording else { return }
        try prepare(withInput: true)
        let input = engine.inputNode
        let inputFormat = input.outputFormat(forBus: 0)
        guard inputFormat.sampleRate > 0, let chunker = PCMChunker(inputFormat: inputFormat) else {
            throw APIError(message: "Micro không hỗ trợ định dạng ghi âm")
        }
        input.installTap(onBus: 0, bufferSize: 2048, format: inputFormat) { buffer, _ in
            chunker.consume(buffer).forEach(onChunk)
        }
        isRecording = true
    }

    func stopRecording() {
        guard isRecording else { return }
        engine.inputNode.removeTap(onBus: 0)
        isRecording = false
    }

    func play(pcm16 data: Data, sampleRate: Int?) {
        var data = data
        // Âm thanh cảnh báo có thể là WAV: bỏ header 44 byte.
        if data.starts(with: Data("RIFF".utf8)), data.count > 44 {
            data = data.dropFirst(44)
        }
        let frames = data.count / 2
        guard frames > 0 else { return }
        do {
            try prepare(withInput: false)
        } catch {
            return
        }
        let rate = Double(sampleRate ?? Self.defaultOutputSampleRate)
        if playerFormat.sampleRate != rate,
           let format = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: rate, channels: 1, interleaved: false) {
            player.stop()
            engine.connect(player, to: engine.mainMixerNode, format: format)
            playerFormat = format
        }
        guard let buffer = AVAudioPCMBuffer(pcmFormat: playerFormat, frameCapacity: AVAudioFrameCount(frames)),
              let channel = buffer.floatChannelData?[0]
        else { return }
        buffer.frameLength = AVAudioFrameCount(frames)
        data.withUnsafeBytes { raw in
            for index in 0..<frames {
                let sample = Int16(littleEndian: raw.loadUnaligned(fromByteOffset: index * 2, as: Int16.self))
                channel[index] = Float(sample) / 32768
            }
        }
        player.scheduleBuffer(buffer)
        if !player.isPlaying {
            player.play()
        }
    }

    private func prepare(withInput: Bool) throws {
        if !isConfigured {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
            try session.setActive(true)
            engine.attach(player)
            engine.connect(player, to: engine.mainMixerNode, format: playerFormat)
            isConfigured = true
        }
        // Chỉ chạm tới inputNode khi cần ghi âm để phát tiếng không đòi quyền micro.
        if withInput && !isVoiceProcessingEnabled {
            // Khử vọng để micro không thu lại tiếng trợ lý phát ra loa; phải bật khi engine dừng.
            engine.stop()
            try? engine.inputNode.setVoiceProcessingEnabled(true)
            isVoiceProcessingEnabled = true
        }
        if !engine.isRunning {
            engine.prepare()
            try engine.start()
        }
    }
}

/// Đổi buffer micro (tần số phần cứng, float) sang PCM16 16 kHz mono và cắt thành từng khúc 100 ms.
private final class PCMChunker {
    private static let chunkBytes = GeminiAudio.inputSampleRate * 2 / 10

    private let converter: AVAudioConverter
    private let outputFormat: AVAudioFormat
    private var pending = Data()

    init?(inputFormat: AVAudioFormat) {
        guard let outputFormat = AVAudioFormat(
            commonFormat: .pcmFormatInt16, sampleRate: Double(GeminiAudio.inputSampleRate), channels: 1, interleaved: true
        ), let converter = AVAudioConverter(from: inputFormat, to: outputFormat) else { return nil }
        self.converter = converter
        self.outputFormat = outputFormat
    }

    func consume(_ buffer: AVAudioPCMBuffer) -> [Data] {
        let ratio = outputFormat.sampleRate / buffer.format.sampleRate
        let capacity = AVAudioFrameCount(Double(buffer.frameLength) * ratio) + 16
        guard let output = AVAudioPCMBuffer(pcmFormat: outputFormat, frameCapacity: capacity) else { return [] }
        var delivered = false
        var error: NSError?
        converter.convert(to: output, error: &error) { _, status in
            if delivered {
                status.pointee = .noDataNow
                return nil
            }
            delivered = true
            status.pointee = .haveData
            return buffer
        }
        guard error == nil, output.frameLength > 0, let samples = output.int16ChannelData else { return [] }
        pending.append(Data(bytes: samples[0], count: Int(output.frameLength) * 2))

        var chunks: [Data] = []
        while pending.count >= Self.chunkBytes {
            chunks.append(pending.prefix(Self.chunkBytes))
            pending.removeFirst(Self.chunkBytes)
        }
        return chunks
    }
}
