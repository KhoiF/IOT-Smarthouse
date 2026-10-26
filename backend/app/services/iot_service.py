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
from app.core.rooms import ROOMS, Relay, Room
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
