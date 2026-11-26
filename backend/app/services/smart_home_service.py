import asyncio
import json
import logging
import threading
import time
import unicodedata
from dataclasses import asdict, dataclass, field
from difflib import SequenceMatcher
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple, Literal

from app.core.config import settings

logger = logging.getLogger(__name__)


@dataclass
class LightState:
    location: str
    is_on: bool = False


class LightingService:
    """Manage the state of smart home lights."""

    def __init__(self, storage_path: Optional[Path] = None) -> None:
        self.storage_path = storage_path or settings.light_state_file
        self.storage_path.parent.mkdir(parents=True, exist_ok=True)
        self._states: Dict[str, LightState] = {}
        self._listeners: List[Tuple[asyncio.AbstractEventLoop, "asyncio.Queue[dict]"]] = []
        self._listener_lock = threading.Lock()
        self._load()

    # ------------------------------------------------------------------
    # Persistence helpers
    # ------------------------------------------------------------------

    def _default_states(self) -> Dict[str, LightState]:
        defaults: Dict[str, LightState] = {}
        for name, is_on in settings.default_lights:
            location_key = name.lower().strip()
            defaults[location_key] = LightState(location=name, is_on=is_on)
        return defaults

    def _load(self) -> None:
        if not self.storage_path.exists():
            self._states = self._default_states()
            self._persist()
            return
        try:
            with self.storage_path.open("r", encoding="utf-8") as file:
                payload = json.load(file)
        except json.JSONDecodeError:
            logger.warning("Corrupted light state file, recreating it with defaults")
            self._states = self._default_states()
            self._persist()
            return

        states: Dict[str, LightState] = {}
        if isinstance(payload, dict):
            for location_key, raw_state in payload.items():
                if isinstance(raw_state, dict):
                    name = raw_state.get("name", location_key)
                    is_on = bool(raw_state.get("is_on"))
                else:
                    name = str(raw_state)
                    is_on = bool(raw_state)
                normalized_key = location_key.lower().strip()
                states[normalized_key] = LightState(location=name, is_on=is_on)
        else:
            logger.warning("Unexpected light state format; recreating defaults")

        # Chỉ giữ các phòng có LED thật trên ESP32, theo thứ tự cấu hình.
        defaults = self._default_states()
        self._states = {
            key: LightState(location=default.location, is_on=states[key].is_on if key in states else default.is_on)
            for key, default in defaults.items()
        }
        if set(states) != set(defaults):
            self._persist()

    def _persist(self) -> None:
        with self.storage_path.open("w", encoding="utf-8") as file:
            json.dump(
                {
                    loc: {"name": state.location, "is_on": state.is_on}
                    for loc, state in self._states.items()
                },
                file,
                ensure_ascii=False,
                indent=2,
            )

    # ------------------------------------------------------------------
    # Observer helpers
    # ------------------------------------------------------------------

    def _notify_listeners(self, payload: dict) -> None:
        with self._listener_lock:
            listeners = list(self._listeners)
        for loop, queue in listeners:
            try:
                loop.call_soon_threadsafe(queue.put_nowait, payload)
            except RuntimeError:
                # event loop might be closed; drop the subscriber
                self.remove_listener(queue)

    async def add_listener(self) -> "asyncio.Queue[dict]":
        loop = asyncio.get_running_loop()
        queue: "asyncio.Queue[dict]" = asyncio.Queue()
        with self._listener_lock:
            self._listeners.append((loop, queue))
        # Provide an immediate snapshot to new listeners
        snapshot = [
            {"location": state.location, "is_on": state.is_on}
            for state in self.get_all_states()
        ]
        await queue.put({"type": "snapshot", "lights": snapshot})
        return queue

    def remove_listener(self, queue: "asyncio.Queue[dict]") -> None:
        with self._listener_lock:
            self._listeners = [
                (loop, q)
                for loop, q in self._listeners
                if q is not queue
            ]

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def toggle_light(self, location: str, turn_on: bool) -> LightState:
        self._load()
        location_key = location.lower().strip()
        display_name = location.strip()
        state = self._states.get(location_key, LightState(location=display_name))
        state.location = display_name
        state.is_on = turn_on
        self._states[location_key] = state
        self._persist()
        logger.info("Light %s is now %s", location_key, "on" if state.is_on else "off")
        self._notify_listeners({"type": "update", "light": asdict(state)})
        return state

    def get_light_state(self, location: str) -> LightState:
        self._load()
        location_key = location.lower().strip()
        state = self._states.get(location_key)
        if state:
            return state
        return LightState(location=location.strip())

    def get_all_states(self) -> Iterable[LightState]:
        self._load()
        return list(self._states.values())


class MusicService:
    """Select a song that best matches the requested title."""

    def __init__(self, music_directory: Optional[Path] = None, threshold: Optional[float] = None) -> None:
        self.music_directory = music_directory or settings.music_directory
        self.threshold = threshold if threshold is not None else settings.music_similarity_threshold
        self.music_directory.mkdir(parents=True, exist_ok=True)
        self._catalog = self._build_catalog()

    def _build_catalog(self) -> List[str]:
        supported_ext = {".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a"}
        catalog = []
        if not self.music_directory.exists():
            return catalog
        for item in self.music_directory.iterdir():
            if item.is_file() and item.suffix.lower() in supported_ext:
                catalog.append(item.stem)
        return catalog

    def refresh(self) -> None:
        self._catalog = self._build_catalog()

    def list_available_songs(self) -> List[str]:
        if not self._catalog:
            self.refresh()
        return list(self._catalog)

    @staticmethod
    def _normalize(text: str) -> str:
        """'Nơi Này  Có Anh' -> 'noi nay co anh' (bỏ dấu, chữ thường)."""
        lowered = text.strip().lower().replace("đ", "d")
        decomposed = unicodedata.normalize("NFD", lowered)
        stripped = "".join(ch for ch in decomposed if unicodedata.category(ch) != "Mn")
        return " ".join(stripped.split())

    def _score(self, query: str, track: str) -> float:
        """Độ khớp 0..1 giữa từ khoá (đã chuẩn hoá) và tên bài."""
        title = self._normalize(track)
        if not query:
            return 0.0
        if title == query:
            return 1.0
        if title.startswith(query):
            return 0.95
        if f" {query}" in f" {title}":
            return 0.9  # khớp từ đầu một từ trong tên bài
        if query in title:
            return 0.85
        whole = SequenceMatcher(a=query, b=title).ratio()
        # So từ khoá với từng đoạn cùng độ dài trong tên bài, để gõ thiếu/sai vài chữ vẫn ra.
        size = len(query)
        partial = max(
            (SequenceMatcher(a=query, b=title[i : i + size]).ratio() for i in range(max(1, len(title) - size + 1))),
            default=0.0,
        )
        return max(whole, partial * 0.8)

    def search(self, query: str, limit: int = 8, min_score: float = 0.65) -> List[Tuple[str, float]]:
        """Gợi ý bài hát theo từ khoá, xếp theo độ khớp giảm dần. Từ khoá trống -> cả thư viện."""
        self.refresh()
        normalized = self._normalize(query)
        if not normalized:
            return [(track, 0.0) for track in sorted(self._catalog, key=self._normalize)[:limit]]
        scored = [(track, self._score(normalized, track)) for track in self._catalog]
        scored = [item for item in scored if item[1] >= min_score]
        scored.sort(key=lambda item: (-item[1], self._normalize(item[0])))
        return scored[:limit]

    def choose_song(self, requested_title: str) -> Optional[str]:
        if not requested_title:
            return None
        results = self.search(requested_title, limit=1, min_score=0.0)
        if not results:
            return None
        best_match, score = results[0]
        logger.info("Best song match for '%s' is '%s' (score %.3f)", requested_title, best_match, score)
        if score >= self.threshold:
            return best_match
        return None

    def find_song_file(self, track_title: str) -> Optional[Path]:
        if not track_title:
            return None
        if not self.music_directory.exists():
            return None
        for item in self.music_directory.iterdir():
            if item.is_file() and item.stem == track_title:
                return item
        return None

    def get_song_duration(self, track_title: str) -> Optional[float]:
        file_path = self.find_song_file(track_title)
        if not file_path:
            return None
        try:
            from mutagen import File  # type: ignore
        except ImportError:
            logger.debug("mutagen library is not available; cannot determine song duration")
            return None

        try:
            audio = File(file_path)
        except Exception:  # pylint: disable=broad-except
            logger.exception("Failed to read audio metadata for %%s", file_path)
            return None

        if audio is None or getattr(audio, "info", None) is None:
            return None

        length = getattr(audio.info, "length", None)
        if length is None:
            return None
        try:
            return float(length)
        except (TypeError, ValueError):
            return None


@dataclass
class MusicPlaybackState:
    requested_title: Optional[str] = None
    matched_song: Optional[str] = None
    status: Literal["stopped", "playing", "paused"] = "stopped"
    position_seconds: float = 0.0
    duration_seconds: Optional[float] = None
    updated_at: float = field(default_factory=lambda: time.time())

    def copy(self) -> "MusicPlaybackState":
        return MusicPlaybackState(
            requested_title=self.requested_title,
            matched_song=self.matched_song,
            status=self.status,
            position_seconds=self.position_seconds,
            duration_seconds=self.duration_seconds,
            updated_at=self.updated_at,
        )

    def effective_position(self, reference_time: Optional[float] = None) -> float:
        base_position = max(self.position_seconds, 0.0)
        if self.status != "playing":
            return base_position
        now = reference_time or time.time()
        elapsed = max(0.0, now - self.updated_at)
        return base_position + elapsed


class SmartHomeService:
    def __init__(
        self,
        lighting_service: Optional[LightingService] = None,
        music_service: Optional[MusicService] = None,
    ) -> None:
        self.lighting_service = lighting_service or LightingService()
        self.music_service = music_service or MusicService()
        self._music_playback_state = MusicPlaybackState()
        self._music_listeners: List[
            Tuple[asyncio.AbstractEventLoop, "asyncio.Queue[dict]"]
        ] = []
        self._music_listener_lock = threading.Lock()

    def turn_on_light(self, location: str) -> LightState:
        return self.lighting_service.toggle_light(location, True)

    def turn_off_light(self, location: str) -> LightState:
        return self.lighting_service.toggle_light(location, False)

    def _notify_music_listeners(self, payload: dict) -> None:
        with self._music_listener_lock:
            listeners = list(self._music_listeners)
        for loop, queue in listeners:
            try:
                loop.call_soon_threadsafe(queue.put_nowait, payload)
            except RuntimeError:
                self.remove_music_listener(queue)

    async def add_music_listener(self) -> "asyncio.Queue[dict]":
        loop = asyncio.get_running_loop()
        queue: "asyncio.Queue[dict]" = asyncio.Queue()
        with self._music_listener_lock:
            self._music_listeners.append((loop, queue))
        await queue.put({"type": "snapshot", "state": self._serialize_music_state()})
        return queue

    def remove_music_listener(self, queue: "asyncio.Queue[dict]") -> None:
        with self._music_listener_lock:
            self._music_listeners = [
                (loop, q) for loop, q in self._music_listeners if q is not queue
            ]

    def _serialize_music_state(self) -> dict:
        state = self.get_music_playback_state()
        return {
            "requested_title": state.requested_title,
            "matched_song": state.matched_song,
            "status": state.status,
            "position_seconds": state.position_seconds,
            "duration_seconds": state.duration_seconds,
            "updated_at": state.updated_at,
        }

    def play_music(self, title: str) -> Optional[str]:
        matched = self.music_service.choose_song(title)
        if matched:
            self._music_playback_state = MusicPlaybackState(
                requested_title=title,
                matched_song=matched,
                status="playing",
                position_seconds=0.0,
                duration_seconds=self.music_service.get_song_duration(matched),
                updated_at=time.time(),
            )
        else:
            self._music_playback_state = MusicPlaybackState(
                requested_title=title,
                matched_song=None,
                status="stopped",
                position_seconds=0.0,
                duration_seconds=None,
                updated_at=time.time(),
            )
        self._notify_music_listeners({"type": "update", "state": self._serialize_music_state()})
        return matched

    def get_song_file(self, title: str) -> Optional[Path]:
        return self.music_service.find_song_file(title)

    def get_lights(self) -> List[LightState]:
        return list(self.lighting_service.get_all_states())

    def list_music(self) -> List[str]:
        return self.music_service.list_available_songs()

    def search_music(self, query: str, limit: int = 8) -> List[str]:
        return [title for title, _ in self.music_service.search(query, limit=limit)]

    def play_exact_song(self, title: str) -> Optional[str]:
        """Phát đúng bài được chọn từ danh sách gợi ý (không đoán gần đúng)."""
        if not self.music_service.find_song_file(title):
            return None
        self._music_playback_state = MusicPlaybackState(
            requested_title=title,
            matched_song=title,
            status="playing",
            position_seconds=0.0,
            duration_seconds=self.music_service.get_song_duration(title),
            updated_at=time.time(),
        )
        self._notify_music_listeners({"type": "update", "state": self._serialize_music_state()})
        return title

    def get_music_playback_state(self) -> MusicPlaybackState:
        state = self._music_playback_state.copy()
        state.position_seconds = state.effective_position()
        state.updated_at = time.time()
        return state

    def pause_music(self) -> bool:
        if (
            self._music_playback_state.matched_song
            and self._music_playback_state.status == "playing"
        ):
            self._music_playback_state.position_seconds = self._music_playback_state.effective_position()
            self._music_playback_state.status = "paused"
            self._music_playback_state.updated_at = time.time()
            self._notify_music_listeners(
                {"type": "update", "state": self._serialize_music_state()}
            )
            return True
        return False

    def continue_music(self) -> bool:
        if (
            self._music_playback_state.matched_song
            and self._music_playback_state.status == "paused"
        ):
            self._music_playback_state.status = "playing"
            self._music_playback_state.updated_at = time.time()
            self._notify_music_listeners(
                {"type": "update", "state": self._serialize_music_state()}
            )
            return True
        return False

    def seek_music(self, position_seconds: float) -> MusicPlaybackState:
        position = max(0.0, position_seconds)
        duration = self._music_playback_state.duration_seconds
        if duration:
            position = min(position, max(0.0, duration - 1.0))
        self._music_playback_state.position_seconds = position
        self._music_playback_state.updated_at = time.time()
        self._notify_music_listeners(
            {"type": "update", "state": self._serialize_music_state()}
        )
        return self.get_music_playback_state()


_global_smart_home_service: Optional[SmartHomeService] = None


def get_smart_home_service() -> SmartHomeService:
    global _global_smart_home_service
    if _global_smart_home_service is None:
        _global_smart_home_service = SmartHomeService()
    return _global_smart_home_service
