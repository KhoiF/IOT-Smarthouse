"""Tải mô hình nhận diện khuôn mặt của OpenCV Zoo về backend/models (không commit vào git).

Chạy từ thư mục backend:  .venv/bin/python -m app.scripts.download_face_models
"""

from __future__ import annotations

import sys
import urllib.request

from app.core.config import settings

BASE_URL = "https://github.com/opencv/opencv_zoo/raw/main/models"
MODELS = {
    settings.face_detector_model: f"{BASE_URL}/face_detection_yunet/face_detection_yunet_2023mar.onnx",
    settings.face_recognizer_model: f"{BASE_URL}/face_recognition_sface/face_recognition_sface_2021dec.onnx",
}


def main() -> int:
    for path, url in MODELS.items():
        if path.exists() and path.stat().st_size > 0:
            print(f"Đã có {path}")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        print(f"Đang tải {url}")
        temp_path = path.with_suffix(".part")
        urllib.request.urlretrieve(url, temp_path)
        temp_path.rename(path)
        print(f"Đã lưu {path} ({path.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
