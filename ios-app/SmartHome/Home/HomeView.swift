import Charts
import SwiftUI

struct HomeView: View {
    @Bindable var model: HomeModel

    var body: some View {
        NavigationStack {
            List {
                if let alarms = model.state?.alarms, alarms.fire || alarms.intrusion {
                    alarmSection(alarms)
                }
                StatusSection(device: model.device, isStreamConnected: model.isStreamConnected)
                SensorSection(device: model.device, state: model.state, history: model.history)
                securitySection
                switchSection(
                    title: "Đèn",
                    items: Catalog.rooms,
                    prefix: "led",
                    values: model.state?.outputs.leds,
                    footer: Catalog.manualOverrideNote,
                    onChange: model.setLed
                )
                switchSection(
                    title: "Relay",
                    items: Catalog.relays,
                    prefix: "relay",
                    values: model.state?.outputs.relays,
                    footer: model.state?.alarms.fire == true ? "Đang cảnh báo cháy: không thể bật relay." : nil,
                    onChange: model.setRelay
                )
                doorSection
            }
            .navigationTitle("Nhà thông minh")
            .refreshable { await model.refresh() }
            .errorAlert($model.errorMessage)
        }
    }

    private func alarmSection(_ alarms: IotAlarms) -> some View {
        Section {
            VStack(alignment: .leading, spacing: 6) {
                Label(alarms.fire ? "CẢNH BÁO CHÁY" : "PHÁT HIỆN XÂM NHẬP", systemImage: "exclamationmark.triangle.fill")
                    .font(.headline)
                    .foregroundStyle(.red)
                Text(alarmDescription(alarms))
                    .font(.subheadline)
                if alarms.silenced {
                    Text("Đã tắt còi, cảnh báo vẫn còn hiệu lực.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            if !alarms.silenced {
                Button(model.pending.contains("silence") ? "Đang gửi…" : "Tắt còi", role: .destructive) {
                    model.silenceAlarm()
                }
                .disabled(!model.isOnline || model.pending.contains("silence"))
            }
        }
        .listRowBackground(Color.red.opacity(0.12))
    }

    private func alarmDescription(_ alarms: IotAlarms) -> String {
        if alarms.fire {
            let cause = alarms.gas ? "Cảm biến khói/gas vượt ngưỡng" : "Nhiệt độ quá cao"
            return "\(cause)\(alarms.gasSimulated ? " (giả lập)" : ""). Đã ngắt quạt và đèn phụ."
        }
        if alarms.doorForced == true {
            return "Cửa bị mở trái phép khi đang bật An ninh. Đã bật đèn phụ."
        }
        return "Cảm biến chuyển động phát hiện người khi đang bật An ninh. Đã bật đèn phụ."
    }

    private var securitySection: some View {
        let security = model.state?.security
        let description = switch (security?.armed, security?.arming) {
        case (true, true): "Đang kích hoạt…"
        case (true, _): "Đang bật: phát hiện chuyển động sẽ hú còi báo trộm."
        default: "Đang tắt: cảm biến chuyển động dùng để tự bật đèn phòng khách."
        }
        return Section {
            Toggle(isOn: Binding(get: { security?.armed ?? false }, set: model.setArmed)) {
                VStack(alignment: .leading, spacing: 2) {
                    Label("Chế độ An ninh", systemImage: security?.armed == true ? "lock.shield.fill" : "lock.shield")
                    Text(description)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .disabled(!model.isOnline || security == nil || model.pending.contains("security"))
        }
    }

    private func switchSection(
        title: String,
        items: [SwitchInfo],
        prefix: String,
        values: [String: Bool]?,
        footer: String?,
        onChange: @escaping (String, Bool) -> Void
    ) -> some View {
        Section {
            ForEach(items) { item in
                SwitchRow(
                    item: item,
                    isOn: values?[item.id],
                    isPending: model.pending.contains("\(prefix):\(item.id)"),
                    isEnabled: model.isOnline && values != nil,
                    onChange: { onChange(item.id, $0) }
                )
            }
        } header: {
            Text(title)
        } footer: {
            if let footer {
                Text(footer)
            }
        }
    }

    private var doorSection: some View {
        let door = model.state?.door
        let isOpen = door?.state == "open" || door?.state == "opening"
        let isBusy = !model.isOnline || model.pending.contains("door")
        return Section {
            LabeledContent("Trạng thái") {
                Text(doorLabel(door))
                    .foregroundStyle(door?.heldByFire == true || door?.state == "not_closed" ? .red : .secondary)
            }
            if let door {
                LabeledContent("Cảm biến") {
                    switch door.sensorClosed {
                    case nil: Text("Chưa lắp")
                    case true?: Text("Cửa đóng")
                    case false?: Text("Cửa hở").foregroundStyle(.red)
                    }
                }
            }
            if door?.state == "not_closed" {
                Label(
                    "Servo đã về vị trí đóng nhưng cảm biến vẫn báo cửa hở. Kiểm tra cửa có bị kẹt hoặc vướng không.",
                    systemImage: "exclamationmark.triangle.fill"
                )
                .font(.footnote)
                .foregroundStyle(.red)
            }
            HStack {
                Button {
                    model.door(open: true)
                } label: {
                    Label("Mở cửa", systemImage: "door.left.hand.open").frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(isBusy)

                Button {
                    model.door(open: false)
                } label: {
                    Label("Đóng cửa", systemImage: "door.left.hand.closed").frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(isBusy || !isOpen || door?.heldByFire == true)
            }
            if let message = model.doorMessage {
                Text(message)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        } header: {
            Text("Cửa ra vào")
        } footer: {
            Text("Mở cửa bằng nút sẽ tắt chế độ An ninh. Nhận diện khuôn mặt chạy trên giao diện web.")
        }
    }

    private func doorLabel(_ door: IotDoor?) -> String {
        guard let door else { return "Chưa có dữ liệu" }
        if door.heldByFire { return "Mở thoát hiểm (cháy)" }
        return Catalog.doorStateLabels[door.state] ?? door.state
    }
}

private struct StatusSection: View {
    let device: IotDeviceInfo?
    let isStreamConnected: Bool

    var body: some View {
        let online = isStreamConnected && device?.online == true
        Section {
            HStack {
                Circle()
                    .fill(online ? .green : .red)
                    .frame(width: 10, height: 10)
                Text(!isStreamConnected ? "Mất kết nối máy chủ" : online ? "ESP32 online" : "ESP32 offline")
                    .fontWeight(.semibold)
                Spacer()
                if let firmware = device?.firmware {
                    Text("Firmware \(firmware)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            TimelineView(.periodic(from: .now, by: 1)) { context in
                Text(lastSeenText(now: context.date))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func lastSeenText(now: Date) -> String {
        guard let lastSeen = device?.lastSeen else { return "Chưa nhận được dữ liệu từ ESP32" }
        let age = max(0, Int(now.timeIntervalSince1970 - lastSeen))
        return "Cập nhật lúc \(Catalog.clock(lastSeen)) (\(age) giây trước)"
    }
}

private struct SwitchRow: View {
    let item: SwitchInfo
    let isOn: Bool?
    let isPending: Bool
    let isEnabled: Bool
    let onChange: (Bool) -> Void

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: isOn == true ? "\(item.symbol).fill" : item.symbol)
                .font(.title2)
                .foregroundStyle(isOn == true ? .yellow : .secondary)
                .frame(width: 32)
            VStack(alignment: .leading, spacing: 2) {
                Text(item.name)
                Text(isPending ? "Đang gửi…" : isOn.map { $0 ? "Đang bật" : "Đang tắt" } ?? "Chưa có dữ liệu")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                if let automation = item.automation {
                    Text(automation)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            Spacer()
            if isPending {
                ProgressView()
            } else {
                Toggle(item.name, isOn: Binding(get: { isOn ?? false }, set: onChange))
                    .labelsHidden()
                    .disabled(!isEnabled)
            }
        }
    }
}

private struct SensorSection: View {
    let device: IotDeviceInfo?
    let state: IotDeviceState?
    let history: [IotHistoryPoint]

    @State private var metric = Metric.temperature

    var body: some View {
        let sensors = state?.sensors
        let alarms = state?.alarms
        Section("Môi trường") {
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                SensorTile(
                    label: "Nhiệt độ",
                    value: sensors.flatMap { $0.dhtOk ? $0.temperatureC : nil }.map { String(format: "%.1f °C", $0) } ?? "--",
                    hint: sensors?.dhtOk == false ? "DHT11 lỗi đọc" : nil,
                    tone: alarms?.heat == true ? .danger : .normal
                )
                SensorTile(
                    label: "Độ ẩm",
                    value: sensors.flatMap { $0.dhtOk ? $0.humidityPct : nil }.map { String(format: "%.0f %%", $0) } ?? "--"
                )
                SensorTile(
                    label: "Khói/gas (MQ-2)",
                    value: sensors.map { String(format: "%.0f", $0.mq2Raw) } ?? "--",
                    hint: sensors.map(gasHint),
                    tone: alarms?.gas == true ? .danger : sensors?.gasLevel == "high" ? .warning : .normal
                )
                SensorTile(label: "Ánh sáng", value: sensors.map { $0.isDark ? "Tối" : "Sáng" } ?? "--")
                SensorTile(
                    label: "Chuyển động",
                    value: sensors.map { $0.motion ? "Có" : "Không" } ?? "--",
                    hint: sensors?.pirReady == false ? "PIR đang khởi động" : nil,
                    tone: alarms?.intrusion == true ? .danger : .normal
                )
                SensorTile(
                    label: "Wi-Fi ESP32",
                    value: device?.online == true ? state.map { "\($0.rssi) dBm" } ?? "--" : "--"
                )
            }
            .padding(.vertical, 4)

            VStack(alignment: .leading, spacing: 8) {
                Picker("Chỉ số", selection: $metric) {
                    ForEach(Metric.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)

                let points = history.compactMap { point in
                    metric.value(of: point).map { ChartPoint(date: Date(timeIntervalSince1970: point.t), value: $0) }
                }
                if points.isEmpty {
                    Text("Chưa có lịch sử đo")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity, minHeight: 160)
                } else {
                    Chart(points) { point in
                        LineMark(x: .value("Thời điểm", point.date), y: .value(metric.rawValue, point.value))
                            .interpolationMethod(.monotone)
                    }
                    .chartYScale(domain: .automatic(includesZero: false))
                    .frame(height: 160)
                }
            }
            .padding(.vertical, 4)
        }
    }

    private func gasHint(_ sensors: IotSensors) -> String {
        if !sensors.mq2Ready { return "Đang khởi động" }
        if state?.alarms.gas == true || sensors.gasLevel == "danger" { return "Nguy hiểm – báo cháy" }
        if sensors.gasLevel == "high" { return "Cao (cảnh báo)" }
        return "Bình thường"
    }
}

private enum Metric: String, CaseIterable, Identifiable {
    case temperature = "Nhiệt độ (°C)"
    case humidity = "Độ ẩm (%)"
    case gas = "MQ-2"

    var id: Self { self }

    func value(of point: IotHistoryPoint) -> Double? {
        switch self {
        case .temperature: point.temperatureC
        case .humidity: point.humidityPct
        case .gas: point.mq2Raw
        }
    }
}

private struct ChartPoint: Identifiable {
    let date: Date
    let value: Double

    var id: Date { date }
}

private struct SensorTile: View {
    enum Tone { case normal, warning, danger }

    let label: String
    let value: String
    var hint: String?
    var tone = Tone.normal

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.title3.weight(.semibold).monospacedDigit())
            if let hint {
                Text(tone == .warning ? "⚠ \(hint)" : hint)
                    .font(.caption)
                    .foregroundStyle(tone == .danger ? .red : tone == .warning ? .orange : .secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(10)
        .background(background, in: RoundedRectangle(cornerRadius: 10))
    }

    private var background: Color {
        switch tone {
        case .normal: Color(.tertiarySystemGroupedBackground)
        case .warning: .orange.opacity(0.15)
        case .danger: .red.opacity(0.15)
        }
    }
}
