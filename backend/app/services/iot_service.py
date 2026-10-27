"""Kết nối với ESP32: nhận telemetry, gửi lệnh qua WebSocket và chờ ACK.

ESP32 là nguồn sự thật: trạng thái đầu ra chỉ đổi khi thiết bị báo lại
(ACK hoặc telemetry), không đổi theo lệnh vừa gửi.
"""

from __future__ import annotations

import asyncio
import logging
import time
import uuid
from collections import deque
from typing import Any, Deque, Dict, Optional, Set

from fastapi import WebSocket

from app.core.config import settings
from app.core.rooms import RELAYS, ROOMS, Relay, Room
from app.services.smart_home_service import LightingService, get_smart_home_service

logger = logging.getLogger(__name__)


class IoTError(Exception):
    status_code = 500

    def __init__(self, message: str) -> None:
        super().__init__(message)
        self.message = message


class DeviceOfflineError(IoTError):
    status_code = 503

    def __init__(self) -> None:
        super().__init__("ESP32 đang offline, chưa thể thực hiện lệnh")


class CommandTimeoutError(IoTError):
    status_code = 504

    def __init__(self) -> None:
        super().__init__("ESP32 không xác nhận lệnh kịp thời")


class CommandRejectedError(IoTError):
    status_code = 409

    _MESSAGES = {
        "blocked_by_fire": "Đang có cảnh báo cháy, không thể bật relay",
        "invalid_target": "Thiết bị không hợp lệ",
        "door_disabled": "ESP32 chưa bật chức năng cửa",
        "door_held_by_fire": "Đang có cảnh báo cháy, cửa được giữ mở để thoát hiểm",
        "unknown_action": "ESP32 không hiểu lệnh",
        "bad_request": "Lệnh gửi tới ESP32 không hợp lệ",
    }

    def __init__(self, reason: Optional[str]) -> None:
        super().__init__(self._MESSAGES.get(reason or "", f"ESP32 từ chối lệnh ({reason})"))
        self.reason = reason


class IoTService:
    def __init__(self, lighting_service: LightingService) -> None:
        self._lighting = lighting_service
        self._device_ws: Optional[WebSocket] = None
        self._device_id: Optional[str] = None
        self._firmware: Optional[str] = None
        self._last_seen: Optional[float] = None
        self._state: Optional[Dict[str, Any]] = None
        self._history: Deque[Dict[str, Any]] = deque(maxlen=settings.iot_history_size)
        self._synced_leds: Dict[str, bool] = {}
        self._pending: Dict[str, "asyncio.Future[Dict[str, Any]]"] = {}
        self._listeners: Set["asyncio.Queue[dict]"] = set()
        self._send_lock = asyncio.Lock()

    # ------------------------------------------------------------------
    # Trạng thái
    # ------------------------------------------------------------------

    @property
    def online(self) -> bool:
        return self._device_ws is not None

    def device_info(self) -> Dict[str, Any]:
        return {
            "online": self.online,
            "device_id": self._device_id,
            "firmware": self._firmware,
            "last_seen": self._last_seen,
        }

    def snapshot(self) -> Dict[str, Any]:
        return {
            "device": self.device_info(),
            "state": self._state,
            "history": list(self._history),
        }

    def assistant_summary(self) -> Dict[str, Any]:
        """Số liệu cảm biến và trạng thái thiết bị dạng dễ đọc cho trợ lý giọng nói."""
        if self._state is None or self._last_seen is None:
            return {"result": "error", "message": "Chưa nhận được dữ liệu nào từ ESP32"}

        now = time.time()
        state = self._state
        sensors = state.get("sensors") or {}
        outputs = state.get("outputs") or {}
        alarms = state.get("alarms") or {}
        age_seconds = round(now - self._last_seen)

        if not sensors.get("mq2_ready"):
            gas_status = "cảm biến đang khởi động"
        elif alarms.get("gas"):
            gas_status = "vượt ngưỡng cảnh báo"
        else:
            gas_status = "bình thường"

        active_alarms = []
        if alarms.get("fire"):
            active_alarms.append(
                "cháy: " + ("khói/gas vượt ngưỡng" if alarms.get("gas") else "nhiệt độ quá cao")
                + (" (giả lập)" if alarms.get("gas_simulated") else "")
            )
        if alarms.get("intrusion"):
            active_alarms.append(
                "cửa bị mở trái phép khi đang bật An ninh"
                if alarms.get("door_forced")
                else "phát hiện chuyển động khi đang bật An ninh"
            )

        leds = outputs.get("leds") or {}
        relays = outputs.get("relays") or {}
        summary: Dict[str, Any] = {
            "result": "success",
            "esp32_online": self.online,
            "data_age_seconds": age_seconds,
            "temperature_c": sensors.get("temperature_c") if sensors.get("dht_ok") else None,
            "humidity_percent": sensors.get("humidity_pct") if sensors.get("dht_ok") else None,
            "temperature_sensor_ok": bool(sensors.get("dht_ok")),
            "light": "tối" if sensors.get("is_dark") else "sáng",
            "motion": (
                "có chuyển động" if sensors.get("motion") else "không có chuyển động"
            ) if sensors.get("pir_ready") else "cảm biến chuyển động đang khởi động",
            "gas_sensor": {
                "status": gas_status,
                "raw_adc": sensors.get("mq2_raw"),
                "note": "giá trị ADC tương đối của MQ-2, không phải nồng độ ppm",
            },
            "lights": {room.name: "bật" if leds.get(room.id) else "tắt" for room in ROOMS},
            "devices": {relay.name: "bật" if relays.get(relay.id) else "tắt" for relay in RELAYS},
            "security_mode": "bật" if (state.get("security") or {}).get("armed") else "tắt",
            "door": self._describe_door(state.get("door") or {}),
            "active_alarms": active_alarms,
        }
        if not self.online or age_seconds > 30:
            summary["warning"] = f"ESP32 không gửi dữ liệu mới, số liệu đã cũ {age_seconds} giây"

        history = self._history_stats(now - 3600)
        if history:
            summary["last_hour"] = history
        return summary

    @staticmethod
    def _describe_door(door: Dict[str, Any]) -> Dict[str, Any]:
        sensor_closed = door.get("sensor_closed")
        return {
            "state": {
                "closed": "đã đóng",
                "opening": "đang mở",
                "open": "đang mở",
                "closing": "đang đóng",
                "not_closed": "chưa đóng hẳn: đã ra lệnh đóng nhưng cảm biến vẫn báo cửa hở (có thể bị kẹt hoặc vướng)",
            }.get(door.get("state"), "không rõ"),
            "held_open_by_fire": bool(door.get("held_by_fire")),
            # Cảm biến cửa (reed switch) là nguồn đáng tin nhất; None = chưa lắp, chỉ biết theo lệnh servo.
            "sensor": "chưa lắp cảm biến cửa" if sensor_closed is None else ("đóng" if sensor_closed else "hở"),
        }

    def _history_stats(self, since: float) -> Optional[Dict[str, Any]]:
        points = [point for point in self._history if point["t"] >= since]
        stats: Dict[str, Any] = {}
        for key, label in (("temperature_c", "temperature_c"), ("humidity_pct", "humidity_percent")):
            values = [point[key] for point in points if isinstance(point.get(key), (int, float))]
            if values:
                stats[label] = {
                    "min": round(min(values), 1),
                    "max": round(max(values), 1),
                    "average": round(sum(values) / len(values), 1),
                    "first": round(values[0], 1),
                    "samples": len(values),
                }
        if not stats:
            return None
        stats["minutes_covered"] = round((time.time() - points[0]["t"]) / 60)
        return stats

    def door_state(self) -> Optional[str]:
        return ((self._state or {}).get("door") or {}).get("state")

    def is_armed(self) -> bool:
        return bool(((self._state or {}).get("security") or {}).get("armed"))

    def check_token(self, token: Optional[str]) -> bool:
        expected = settings.iot_device_token
        return not expected or token == expected

    def apply_device_state(
        self, state: Dict[str, Any], device_id: Optional[str] = None, firmware: Optional[str] = None
    ) -> None:
        now = time.time()
        if device_id:
            self._device_id = device_id
        if firmware:
            self._firmware = firmware
        self._last_seen = now
        self._state = state

        history_point = self._maybe_record_history(state, now)
        self._sync_lighting(state)
        self._broadcast({"type": "state", **self.snapshot_without_history(), "history_point": history_point})

    def snapshot_without_history(self) -> Dict[str, Any]:
        return {"device": self.device_info(), "state": self._state}

    def _maybe_record_history(self, state: Dict[str, Any], now: float) -> Optional[Dict[str, Any]]:
        sensors = state.get("sensors") or {}
        if not sensors.get("dht_ok"):
            return None
        if self._history and now - self._history[-1]["t"] < settings.iot_history_interval_seconds:
            return None
        point = {
            "t": now,
            "temperature_c": sensors.get("temperature_c"),
            "humidity_pct": sensors.get("humidity_pct"),
            "mq2_raw": sensors.get("mq2_raw"),
        }
        self._history.append(point)
        return point

    def _sync_lighting(self, state: Dict[str, Any]) -> None:
        """Đồng bộ LightingService (dùng bởi API đèn cũ và Gemini) theo LED thật trên ESP32."""
        leds = (state.get("outputs") or {}).get("leds") or {}
        for room in ROOMS:
            is_on = leds.get(room.id)
            if not isinstance(is_on, bool) or self._synced_leds.get(room.id) == is_on:
                continue
            self._synced_leds[room.id] = is_on
            if self._lighting.get_light_state(room.name).is_on != is_on:
                self._lighting.toggle_light(room.name, is_on)

    # ------------------------------------------------------------------
    # Kết nối thiết bị
    # ------------------------------------------------------------------

    async def device_connected(self, websocket: WebSocket, hello: Dict[str, Any]) -> None:
        previous = self._device_ws
        self._device_ws = websocket
        if previous is not None and previous is not websocket:
            logger.info("ESP32 kết nối lại, đóng kết nối cũ")
            try:
                await previous.close(code=1012)
            except Exception:  # pylint: disable=broad-except
                pass
        logger.info(
            "ESP32 %s (firmware %s) đã kết nối, chạy được %.0f s, lý do khởi động gần nhất: %s",
            hello.get("device_id"),
            hello.get("firmware"),
            (hello.get("boot_uptime_ms") or 0) / 1000,
            hello.get("reset_reason"),
        )
        self.apply_device_state(
            hello.get("state") or {}, hello.get("device_id"), hello.get("firmware")
        )

    def device_disconnected(self, websocket: WebSocket) -> None:
        if self._device_ws is not websocket:
            return
        self._device_ws = None
        for future in self._pending.values():
            if not future.done():
                future.set_exception(DeviceOfflineError())
        self._pending.clear()
        logger.info("ESP32 đã ngắt kết nối")
        self._broadcast({"type": "state", **self.snapshot_without_history(), "history_point": None})

    def handle_device_message(self, message: Dict[str, Any]) -> None:
        if message.get("type") == "ack":
            future = self._pending.get(str(message.get("command_id")))
            if future and not future.done():
                future.set_result(message)
        elif message.get("type") == "state" and isinstance(message.get("state"), dict):
            self.apply_device_state(message["state"])

    # ------------------------------------------------------------------
    # Lệnh
    # ------------------------------------------------------------------

    async def set_led(self, room: Room, on: bool) -> Dict[str, Any]:
        return await self._send_command("set_led", id=room.id, on=on)

    async def set_relay(self, relay: Relay, on: bool) -> Dict[str, Any]:
        return await self._send_command("set_relay", id=relay.id, on=on)

    async def set_armed(self, armed: bool) -> Dict[str, Any]:
        return await self._send_command("set_armed", armed=armed)

    async def silence_alarm(self) -> Dict[str, Any]:
        return await self._send_command("silence")

    async def open_door(self) -> Dict[str, Any]:
        return await self._send_command("open_door")

    async def close_door(self) -> Dict[str, Any]:
        return await self._send_command("close_door")

    async def _send_command(self, action: str, **params: Any) -> Dict[str, Any]:
        websocket = self._device_ws
        if websocket is None:
            raise DeviceOfflineError()

        command_id = uuid.uuid4().hex[:12]
        future: "asyncio.Future[Dict[str, Any]]" = asyncio.get_running_loop().create_future()
        self._pending[command_id] = future
        try:
            async with self._send_lock:
                await websocket.send_json(
                    {"type": "command", "command_id": command_id, "action": action, **params}
                )
            ack = await asyncio.wait_for(future, timeout=settings.iot_ack_timeout_seconds)
        except asyncio.TimeoutError as exc:
            raise CommandTimeoutError() from exc
        except IoTError:
            raise
        except Exception as exc:  # pylint: disable=broad-except
            logger.warning("Không gửi được lệnh tới ESP32: %s", exc)
            raise DeviceOfflineError() from exc
        finally:
            self._pending.pop(command_id, None)

        if isinstance(ack.get("state"), dict):
            self.apply_device_state(ack["state"])
        if not ack.get("ok"):
            raise CommandRejectedError(ack.get("error"))
        return self.snapshot_without_history()

    # ------------------------------------------------------------------
    # Người nghe (web/Android)
    # ------------------------------------------------------------------

    async def add_listener(self) -> "asyncio.Queue[dict]":
        queue: "asyncio.Queue[dict]" = asyncio.Queue()
        self._listeners.add(queue)
        await queue.put({"type": "snapshot", **self.snapshot()})
        return queue

    def remove_listener(self, queue: "asyncio.Queue[dict]") -> None:
        self._listeners.discard(queue)

    def _broadcast(self, message: Dict[str, Any]) -> None:
        for queue in list(self._listeners):
            queue.put_nowait(message)


_global_iot_service: Optional[IoTService] = None


def get_iot_service() -> IoTService:
    global _global_iot_service
    if _global_iot_service is None:
        _global_iot_service = IoTService(get_smart_home_service().lighting_service)
    return _global_iot_service
