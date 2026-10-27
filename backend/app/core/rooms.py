"""Danh sách phòng và relay dùng chung với firmware (esp32-firmware/include/config.h), web và iOS."""

from __future__ import annotations

import unicodedata
from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Room:
    id: str
    name: str


@dataclass(frozen=True)
class Relay:
    id: str
    name: str


# Thứ tự trùng với thứ tự LED trên ESP32 (GPIO 18, 19, 21, 22, 23).
ROOMS: tuple[Room, ...] = (
    Room("phong_khach", "Phòng khách"),
    Room("phong_ngu", "Phòng ngủ"),
    Room("nha_bep", "Nhà bếp"),
    Room("ban_cong", "Ban công"),
    Room("nha_tam", "Nhà tắm"),
)

RELAYS: tuple[Relay, ...] = (
    Relay("relay1", "Quạt"),
    Relay("relay2", "Đèn phụ"),
)


def normalize_key(text: str) -> str:
    """'Phòng Khách ' -> 'phong_khach'."""
    lowered = text.strip().lower().replace("đ", "d")
    decomposed = unicodedata.normalize("NFD", lowered)
    stripped = "".join(ch for ch in decomposed if unicodedata.category(ch) != "Mn")
    return "_".join(stripped.split())


def find_room(text: str) -> Optional[Room]:
    """Tìm phòng theo id hoặc tên tiếng Việt (có/không dấu)."""
    key = normalize_key(text)
    return next((room for room in ROOMS if room.id == key), None)


def find_relay(relay_id: str) -> Optional[Relay]:
    key = normalize_key(relay_id)
    return next((relay for relay in RELAYS if relay.id == key), None)
