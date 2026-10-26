import base64
import binascii
from typing import Any, Dict, List

from fastapi import APIRouter, HTTPException, Request
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field

from app.services.door_service import get_door_service
from app.services.face_service import FaceServiceError, get_face_service

router = APIRouter(tags=["face"])

face_service = get_face_service()
door_service = get_door_service()

MAX_IMAGE_BYTES = 4 * 1024 * 1024


class EnrollRequest(BaseModel):
    name: str = Field(min_length=1, max_length=60)
    # Ảnh JPEG/PNG dạng base64 (có hoặc không có tiền tố data:image/...;base64,)
    images: List[str] = Field(min_length=1, max_length=20)


def _raise(error: FaceServiceError) -> None:
    raise HTTPException(status_code=error.status_code, detail=error.message) from error


@router.get("/face/status")
def face_status() -> Dict[str, Any]:
    return face_service.status()


@router.post("/face/recognize")
async def recognize(request: Request) -> Dict[str, Any]:
    """Nhận một khung hình JPEG (body thô), trả các khuôn mặt và quyết định mở cửa."""
    image = await request.body()
    if not image or len(image) > MAX_IMAGE_BYTES:
        raise HTTPException(status_code=400, detail="Cần gửi một ảnh JPEG/PNG dưới 4 MB")
    try:
        analysis = await run_in_threadpool(face_service.analyze, image)
    except FaceServiceError as error:
        _raise(error)
    analysis["door"] = await door_service.handle_recognition(analysis)
    return analysis


@router.get("/face/people")
def list_people() -> List[Dict[str, Any]]:
    return face_service.list_people()


@router.post("/face/people", status_code=201)
async def enroll(payload: EnrollRequest) -> Dict[str, Any]:
    images: List[bytes] = []
    for encoded in payload.images:
        try:
            images.append(base64.b64decode(encoded.split(",", 1)[-1], validate=True))
        except (binascii.Error, ValueError) as error:
            raise HTTPException(status_code=400, detail="Ảnh mẫu không phải base64 hợp lệ") from error
    try:
        return await run_in_threadpool(face_service.enroll, payload.name, images)
    except FaceServiceError as error:
        _raise(error)


@router.delete("/face/people/{person_id}", status_code=204)
def delete_person(person_id: str) -> None:
    if not face_service.delete_person(person_id):
        raise HTTPException(status_code=404, detail="Không có thành viên này")


@router.get("/face/people/{person_id}/thumbnail")
def person_thumbnail(person_id: str) -> FileResponse:
    path = face_service.thumbnail_path(person_id)
    if path is None:
        raise HTTPException(status_code=404, detail="Không có ảnh")
    return FileResponse(path, media_type="image/jpeg")


@router.post("/door/open")
async def open_door() -> Dict[str, Any]:
    result = await door_service.open_door("manual")
    if result["action"] == "error":
        raise HTTPException(status_code=503, detail=result["message"])
    return result


@router.get("/door/log")
def door_log() -> List[Dict[str, Any]]:
    return door_service.recent_log()
