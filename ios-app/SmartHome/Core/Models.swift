import Foundation

// Hợp đồng với backend /iot/* và firmware ESP32 (esp32-firmware/src/server_link.cpp),
// cùng tên trường với frontend/src/types/iot.ts. CodingKeys viết tay vì
// convertFromSnakeCase cũng đổi khóa dictionary (phong_khach -> phongKhach).

/// Dùng chung cho `GET /iot/state`, phản hồi lệnh và message của `WS /ws/iot/updates`
/// (`snapshot` có `history`, `state` có `history_point`).
struct IotPayload: Decodable {
    let type: String?
    let device: IotDeviceInfo
    let state: IotDeviceState?
    let history: [IotHistoryPoint]?
    let historyPoint: IotHistoryPoint?

    enum CodingKeys: String, CodingKey {
        case type, device, state, history
        case historyPoint = "history_point"
    }
}

struct IotDeviceInfo: Decodable {
    let online: Bool
    let deviceId: String?
    let firmware: String?
    let lastSeen: Double?

    enum CodingKeys: String, CodingKey {
        case online, firmware
        case deviceId = "device_id"
        case lastSeen = "last_seen"
    }
}

struct IotDeviceState: Decodable {
    let rssi: Int
    let sensors: IotSensors
    let outputs: IotOutputs
    let security: IotSecurity
    let alarms: IotAlarms
    let door: IotDoor?
}

struct IotSensors: Decodable {
    let temperatureC: Double?
    let humidityPct: Double?
    let dhtOk: Bool
    let mq2Raw: Double
    let mq2Ready: Bool
    /// normal / high / danger
    let gasLevel: String?
    let motion: Bool
    let pirReady: Bool
    let isDark: Bool

    enum CodingKeys: String, CodingKey {
        case motion
        case temperatureC = "temperature_c"
        case humidityPct = "humidity_pct"
        case dhtOk = "dht_ok"
        case mq2Raw = "mq2_raw"
        case mq2Ready = "mq2_ready"
        case gasLevel = "gas_level"
        case pirReady = "pir_ready"
        case isDark = "is_dark"
    }
}

struct IotOutputs: Decodable {
    let leds: [String: Bool]
    let relays: [String: Bool]
    let buzzer: Bool
}

struct IotSecurity: Decodable {
    let armed: Bool
    let arming: Bool
}

struct IotDoor: Decodable {
    /// closed / opening / open / closing / not_closed
    let state: String
    let heldByFire: Bool
    /// nil khi chưa lắp reed switch (`DOOR_SENSOR_ENABLED` tắt).
    let sensorClosed: Bool?

    enum CodingKeys: String, CodingKey {
        case state
        case heldByFire = "held_by_fire"
        case sensorClosed = "sensor_closed"
    }
}

struct IotAlarms: Decodable {
    let fire: Bool
    let gas: Bool
    let heat: Bool
    let intrusion: Bool
    let doorForced: Bool?
    let silenced: Bool
    let gasSimulated: Bool

    enum CodingKeys: String, CodingKey {
        case fire, gas, heat, intrusion, silenced
        case doorForced = "door_forced"
        case gasSimulated = "gas_simulated"
    }
}

struct IotHistoryPoint: Decodable {
    let t: Double
    let temperatureC: Double?
    let humidityPct: Double?
    let mq2Raw: Double?

    enum CodingKeys: String, CodingKey {
        case t
        case temperatureC = "temperature_c"
        case humidityPct = "humidity_pct"
        case mq2Raw = "mq2_raw"
    }
}

/// Phản hồi của `/door/open` và `/door/close`.
struct DoorResult: Decodable {
    let disarmed: Bool?
}

// MARK: - Nhạc (/smart-home/music/*)

struct MusicPlaybackState: Decodable {
    let requestedTitle: String?
    let matchedSong: String?
    /// playing / paused / stopped
    let status: String
    let positionSeconds: Double
    let durationSeconds: Double?

    enum CodingKeys: String, CodingKey {
        case status
        case requestedTitle = "requested_title"
        case matchedSong = "matched_song"
        case positionSeconds = "position_seconds"
        case durationSeconds = "duration_seconds"
    }
}

struct MusicPlayResponse: Decodable {
    let selectedSong: String
    let playbackState: MusicPlaybackState

    enum CodingKeys: String, CodingKey {
        case selectedSong = "selected_song"
        case playbackState = "playback_state"
    }
}

struct MusicStreamMessage: Decodable {
    let type: String
    let state: MusicPlaybackState?
}
