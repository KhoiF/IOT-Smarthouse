"""Quyết định mở cửa từ kết quả nhận diện khuôn mặt và ghi nhật ký cửa.

Cửa chỉ mở khi cùng một thành viên được nhận ra ở FACE_CONFIRM_FRAMES khung hình trong
FACE_CONFIRM_WINDOW_SECONDS, để một khung hình nhận nhầm không mở được cửa. Mở cửa không
tắt chế độ An ninh.
"""

from __future__ import annotations

import json
import logging
import time
from collections import deque
from typing import Any, Deque, Dict, List, Optional, Tuple

from app.core.config import settings
from app.services.iot_service import IoTError, IoTService, get_iot_service

logger = logging.getLogger(__name__)

LOG_LIMIT = 200
STRANGER_LOG_INTERVAL_SECONDS = 10.0


class DoorService:
    def __init__(self, iot_service: IoTService) -> None:
        self._iot = iot_service
        self._hits: Deque[Tuple[float, str, str]] = deque()
        self._last_open_at = 0.0
        self._last_stranger_log_at = 0.0
        self._log: List[Dict[str, Any]] = self._load_log()

    async def handle_recognition(self, analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Nhận kết quả của FaceService.analyze, mở cửa nếu đủ điều kiện."""
        now = time.time()
        faces = analysis.get("faces") or []
        members = [face for face in faces if face.get("person_id")]

        while self._hits and now - self._hits[0][0] > settings.face_confirm_window_seconds:
            self._hits.popleft()

        if not members:
            if faces and now - self._last_stranger_log_at >= STRANGER_LOG_INTERVAL_SECONDS:
                self._last_stranger_log_at = now
                self._append_log("stranger", None, faces[0].get("similarity"))
            return {"action": "none" if not faces else "stranger"}

        best = max(members, key=lambda face: face.get("similarity") or 0)
        self._hits.append((now, best["person_id"], best["name"]))
        confirmed = sum(1 for _, person_id, _ in self._hits if person_id == best["person_id"])
        if confirmed < settings.face_confirm_frames:
            return {"action": "confirming", "name": best["name"], "frames": confirmed}

        if now - self._last_open_at < settings.door_open_cooldown_seconds:
            return {"action": "cooldown", "name": best["name"]}

        self._hits.clear()
        return await self.open_door("face", best["name"], best.get("similarity"))

    async def open_door(
        self, source: str, name: Optional[str] = None, similarity: Optional[float] = None
    ) -> Dict[str, Any]:
        try:
            await self._iot.open_door()
        except IoTError as error:
            self._append_log("open_failed", name, similarity, error.message)
            return {"action": "error", "name": name, "message": error.message}
        self._last_open_at = time.time()
        self._append_log("face_open" if source == "face" else "manual_open", name, similarity)
        return {"action": "opened", "name": name}

    def recent_log(self, limit: int = 20) -> List[Dict[str, Any]]:
        return list(reversed(self._log[-limit:]))

    def last_event_summary(self) -> Optional[Dict[str, Any]]:
        if not self._log:
            return None
        entry = self._log[-1]
        return {
            "event": {
                "face_open": "mở cửa bằng nhận diện khuôn mặt",
                "manual_open": "mở cửa bằng nút trên ứng dụng",
                "stranger": "người lạ đứng trước camera, không mở cửa",
                "open_failed": "mở cửa thất bại",
            }.get(entry["event"], entry["event"]),
            "name": entry.get("name"),
            "seconds_ago": round(time.time() - entry["t"]),
        }

    def _append_log(
        self, event: str, name: Optional[str], similarity: Optional[float], message: Optional[str] = None
    ) -> None:
        entry: Dict[str, Any] = {"t": time.time(), "event": event, "name": name, "similarity": similarity}
        if message:
            entry["message"] = message
        self._log.append(entry)
        del self._log[:-LOG_LIMIT]
        logger.info("Nhật ký cửa: %s %s", event, name or "")
        try:
            settings.door_log_file.parent.mkdir(parents=True, exist_ok=True)
            settings.door_log_file.write_text(json.dumps(self._log, ensure_ascii=False), encoding="utf-8")
        except OSError:
            logger.exception("Không ghi được nhật ký cửa")

    @staticmethod
    def _load_log() -> List[Dict[str, Any]]:
        try:
            payload = json.loads(settings.door_log_file.read_text(encoding="utf-8"))
            return payload if isinstance(payload, list) else []
        except (OSError, ValueError):
            return []


_global_door_service: Optional[DoorService] = None


def get_door_service() -> DoorService:
    global _global_door_service
    if _global_door_service is None:
        _global_door_service = DoorService(get_iot_service())
    return _global_door_service
