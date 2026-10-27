// Hợp đồng với backend /iot/* và firmware ESP32 (esp32-firmware/src/server_link.cpp).

export interface IotSensors {
  temperature_c: number | null;
  humidity_pct: number | null;
  dht_ok: boolean;
  mq2_raw: number;
  mq2_ready: boolean;
  motion: boolean;
  pir_ready: boolean;
  is_dark: boolean;
}

export interface IotOutputs {
  leds: Record<string, boolean>;
  relays: Record<string, boolean>;
  buzzer: boolean;
}

export interface IotSecurity {
  armed: boolean;
  arming: boolean;
}

export interface IotDoor {
  state: "closed" | "opening" | "open" | "closing" | "not_closed";
  held_by_fire: boolean;
  sensor_closed: boolean | null;
}

export interface IotAlarms {
  fire: boolean;
  gas: boolean;
  heat: boolean;
  intrusion: boolean;
  door_forced?: boolean;
  silenced: boolean;
  gas_simulated: boolean;
}

export interface IotDeviceState {
  uptime_ms: number;
  rssi: number;
  sensors: IotSensors;
  outputs: IotOutputs;
  security: IotSecurity;
  alarms: IotAlarms;
  door?: IotDoor;
}

export interface IotDeviceInfo {
  online: boolean;
  device_id: string | null;
  firmware: string | null;
  last_seen: number | null;
}

export interface IotHistoryPoint {
  t: number;
  temperature_c: number | null;
  humidity_pct: number | null;
  mq2_raw: number | null;
}

export interface IotCommandResponse {
  device: IotDeviceInfo;
  state: IotDeviceState | null;
}

export interface IotSnapshot extends IotCommandResponse {
  history: IotHistoryPoint[];
}

export type IotUpdateMessage =
  | ({ type: "snapshot" } & IotSnapshot)
  | ({ type: "state"; history_point: IotHistoryPoint | null } & IotCommandResponse);
