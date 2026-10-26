import asyncio
import logging
from typing import Any, Dict, Optional

from fastapi import APIRouter, Header, HTTPException, WebSocket, WebSocketDisconnect, status
from pydantic import BaseModel, ConfigDict

from app.core.rooms import find_relay, find_room
from app.services.iot_service import IoTError, get_iot_service

logger = logging.getLogger(__name__)

router = APIRouter(tags=["iot"])

iot_service = get_iot_service()

HELLO_TIMEOUT_SECONDS = 10


class TelemetryPayload(BaseModel):
    model_config = ConfigDict(extra="allow")

    device_id: str
    firmware: Optional[str] = None
    state: Dict[str, Any]


class SwitchRequest(BaseModel):
    on: bool


class SecurityRequest(BaseModel):
    armed: bool


async def _run_command(coro) -> Dict[str, Any]:
    try:
        return await coro
    except IoTError as error:
        raise HTTPException(status_code=error.status_code, detail=error.message) from error


@router.post("/iot/telemetry", status_code=status.HTTP_204_NO_CONTENT)
async def receive_telemetry(
    payload: TelemetryPayload, x_device_token: Optional[str] = Header(default=None)
) -> None:
    if not iot_service.check_token(x_device_token):
        raise HTTPException(status_code=401, detail="Sai token thiết bị")
    iot_service.apply_device_state(payload.state, payload.device_id, payload.firmware)


@router.get("/iot/state")
def get_state() -> Dict[str, Any]:
    return iot_service.snapshot()


@router.post("/iot/leds/{room_id}")
async def set_led(room_id: str, payload: SwitchRequest) -> Dict[str, Any]:
    room = find_room(room_id)
    if room is None:
        raise HTTPException(status_code=404, detail=f"Không có phòng '{room_id}'")
    return await _run_command(iot_service.set_led(room, payload.on))


@router.post("/iot/relays/{relay_id}")
async def set_relay(relay_id: str, payload: SwitchRequest) -> Dict[str, Any]:
    relay = find_relay(relay_id)
    if relay is None:
        raise HTTPException(status_code=404, detail=f"Không có relay '{relay_id}'")
    return await _run_command(iot_service.set_relay(relay, payload.on))


@router.post("/iot/security")
async def set_security(payload: SecurityRequest) -> Dict[str, Any]:
    return await _run_command(iot_service.set_armed(payload.armed))


@router.post("/iot/alarm/silence")
async def silence_alarm() -> Dict[str, Any]:
    return await _run_command(iot_service.silence_alarm())


@router.websocket("/ws/iot/device")
async def device_socket(websocket: WebSocket) -> None:
    """ESP32 gửi `hello` ngay sau khi kết nối, sau đó nhận `command` và trả `ack`."""
    await websocket.accept()
    try:
        hello = await asyncio.wait_for(websocket.receive_json(), timeout=HELLO_TIMEOUT_SECONDS)
    except (asyncio.TimeoutError, WebSocketDisconnect, ValueError):
        await websocket.close(code=1008)
        return

    if hello.get("type") != "hello" or not iot_service.check_token(hello.get("token")):
        logger.warning("Từ chối kết nối thiết bị: hello không hợp lệ hoặc sai token")
        await websocket.close(code=1008)
        return

    await iot_service.device_connected(websocket, hello)
    try:
        while True:
            message = await websocket.receive_json()
            if isinstance(message, dict):
                iot_service.handle_device_message(message)
    except (WebSocketDisconnect, RuntimeError):
        pass
    except ValueError:
        logger.warning("ESP32 gửi dữ liệu không phải JSON")
    finally:
        iot_service.device_disconnected(websocket)


@router.websocket("/ws/iot/updates")
async def updates_socket(websocket: WebSocket) -> None:
    await websocket.accept()
    queue = await iot_service.add_listener()
    try:
        while True:
            message = await queue.get()
            await websocket.send_json(message)
    except (WebSocketDisconnect, RuntimeError):
        pass
    finally:
        iot_service.remove_listener(queue)
