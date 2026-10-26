"""Nhận diện khuôn mặt bằng OpenCV: YuNet tìm mặt, SFace tạo vector đặc trưng để so khớp.

Dữ liệu thành viên (vector + ảnh nhỏ) nằm trong settings.face_data_directory và không được commit.
"""

from __future__ import annotations

import json
import logging
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

import cv2
import numpy as np

from app.core.config import settings

logger = logging.getLogger(__name__)

# Ảnh lớn hơn sẽ được thu nhỏ trước khi tìm mặt để giữ tốc độ.
MAX_DETECT_DIMENSION = 640
DETECT_SCORE_THRESHOLD = 0.8
MIN_ENROLL_SAMPLES = 3


class FaceServiceError(Exception):
    def __init__(self, message: str, status_code: int = 400) -> None:
        super().__init__(message)
        self.message = message
        self.status_code = status_code


@dataclass
class Person:
    id: str
    name: str
    created_at: float
    embeddings: List[List[float]] = field(default_factory=list)

    def public(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "created_at": self.created_at,
            "samples": len(self.embeddings),
        }


class FaceService:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._detector: Optional[Any] = None
        self._recognizer: Optional[Any] = None
        self._data_dir: Path = settings.face_data_directory
        self._people_file = self._data_dir / "people.json"
        self._people: Dict[str, Person] = {}
        self._load_people()

    # ------------------------------------------------------------------
    # Mô hình
    # ------------------------------------------------------------------

    def status(self) -> Dict[str, Any]:
        missing = [
            str(path)
            for path in (settings.face_detector_model, settings.face_recognizer_model)
            if not path.exists()
        ]
        return {
            "ready": not missing,
            "missing_models": missing,
            "threshold": settings.face_match_threshold,
            "people": len(self._people),
        }

    def _ensure_models(self) -> None:
        if self._detector is not None and self._recognizer is not None:
            return
        if not self.status()["ready"]:
            raise FaceServiceError(
                "Chưa có mô hình nhận diện khuôn mặt. Chạy: .venv/bin/python -m app.scripts.download_face_models",
                status_code=503,
            )
        self._detector = cv2.FaceDetectorYN.create(
            str(settings.face_detector_model), "", (320, 320), DETECT_SCORE_THRESHOLD
        )
        self._recognizer = cv2.FaceRecognizerSF.create(str(settings.face_recognizer_model), "")
        logger.info("Đã nạp mô hình nhận diện khuôn mặt")

    # ------------------------------------------------------------------
    # Xử lý ảnh
    # ------------------------------------------------------------------

    @staticmethod
    def _decode(image_bytes: bytes) -> np.ndarray:
        image = cv2.imdecode(np.frombuffer(image_bytes, dtype=np.uint8), cv2.IMREAD_COLOR)
        if image is None:
            raise FaceServiceError("Không đọc được ảnh (cần JPEG hoặc PNG)")
        height, width = image.shape[:2]
        scale = MAX_DETECT_DIMENSION / max(height, width)
        if scale < 1:
            image = cv2.resize(image, (int(width * scale), int(height * scale)))
        return image

    def _detect(self, image: np.ndarray) -> np.ndarray:
        height, width = image.shape[:2]
        self._detector.setInputSize((width, height))
        _, faces = self._detector.detect(image)
        return faces if faces is not None else np.empty((0, 15), dtype=np.float32)

    def _embed(self, image: np.ndarray, face: np.ndarray) -> np.ndarray:
        aligned = self._recognizer.alignCrop(image, face)
        feature = self._recognizer.feature(aligned)
        return feature.flatten() / (np.linalg.norm(feature) or 1.0)

    def _best_match(self, embedding: np.ndarray) -> tuple[Optional[Person], float]:
        best_person: Optional[Person] = None
        best_score = -1.0
        for person in self._people.values():
            if not person.embeddings:
                continue
            scores = np.asarray(person.embeddings, dtype=np.float32) @ embedding
            score = float(scores.max())
            if score > best_score:
                best_person, best_score = person, score
        return best_person, best_score

    def analyze(self, image_bytes: bytes) -> Dict[str, Any]:
        """Tìm mọi khuôn mặt trong ảnh và gán tên nếu giống thành viên đã đăng ký."""
        with self._lock:
            self._ensure_models()
            image = self._decode(image_bytes)
            height, width = image.shape[:2]
            results = []
            for face in self._detect(image):
                person, score = self._best_match(self._embed(image, face))
                matched = person is not None and score >= settings.face_match_threshold
                x, y, w, h = (float(v) for v in face[:4])
                results.append(
                    {
                        # Tọa độ theo tỉ lệ 0..1 để client vẽ khung trên video ở mọi kích thước.
                        "box": [x / width, y / height, w / width, h / height],
                        "detection_score": round(float(face[14]), 3),
                        "person_id": person.id if matched else None,
                        "name": person.name if matched else None,
                        "similarity": round(score, 3) if person else None,
                    }
                )
            return {"width": width, "height": height, "faces": results}

    # ------------------------------------------------------------------
    # Thành viên
    # ------------------------------------------------------------------

    def list_people(self) -> List[Dict[str, Any]]:
        return [person.public() for person in sorted(self._people.values(), key=lambda p: p.created_at)]

    def enroll(self, name: str, images: List[bytes]) -> Dict[str, Any]:
        name = name.strip()
        if not name:
            raise FaceServiceError("Cần nhập tên thành viên")
        if len(images) < MIN_ENROLL_SAMPLES:
            raise FaceServiceError(f"Cần ít nhất {MIN_ENROLL_SAMPLES} ảnh mẫu")

        with self._lock:
            self._ensure_models()
            embeddings: List[List[float]] = []
            thumbnail: Optional[np.ndarray] = None
            for image_bytes in images:
                image = self._decode(image_bytes)
                faces = self._detect(image)
                if len(faces) == 0:
                    continue
                # Lấy khuôn mặt lớn nhất trong ảnh mẫu.
                face = max(faces, key=lambda f: f[2] * f[3])
                embeddings.append(self._embed(image, face).tolist())
                if thumbnail is None:
                    thumbnail = self._recognizer.alignCrop(image, face)

            if len(embeddings) < MIN_ENROLL_SAMPLES:
                raise FaceServiceError(
                    f"Chỉ tìm thấy khuôn mặt trong {len(embeddings)}/{len(images)} ảnh; "
                    f"cần ít nhất {MIN_ENROLL_SAMPLES}. Hãy nhìn thẳng camera, đủ sáng."
                )

            person = Person(id=uuid.uuid4().hex[:10], name=name, created_at=time.time(), embeddings=embeddings)
            self._people[person.id] = person
            self._save_people()
            if thumbnail is not None:
                cv2.imwrite(str(self._data_dir / f"{person.id}.jpg"), thumbnail)
            logger.info("Đã đăng ký khuôn mặt %s (%d mẫu)", name, len(embeddings))
            return person.public()

    def delete_person(self, person_id: str) -> bool:
        with self._lock:
            person = self._people.pop(person_id, None)
            if person is None:
                return False
            self._save_people()
            (self._data_dir / f"{person_id}.jpg").unlink(missing_ok=True)
            logger.info("Đã xóa khuôn mặt %s", person.name)
            return True

    def thumbnail_path(self, person_id: str) -> Optional[Path]:
        path = self._data_dir / f"{person_id}.jpg"
        return path if person_id in self._people and path.exists() else None

    def _load_people(self) -> None:
        if not self._people_file.exists():
            return
        try:
            payload = json.loads(self._people_file.read_text(encoding="utf-8"))
            self._people = {item["id"]: Person(**item) for item in payload}
        except (ValueError, KeyError, TypeError):
            logger.warning("File khuôn mặt bị hỏng, bỏ qua: %s", self._people_file)

    def _save_people(self) -> None:
        self._data_dir.mkdir(parents=True, exist_ok=True)
        payload = [person.__dict__ for person in self._people.values()]
        self._people_file.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")


_global_face_service: Optional[FaceService] = None


def get_face_service() -> FaceService:
    global _global_face_service
    if _global_face_service is None:
        _global_face_service = FaceService()
    return _global_face_service
