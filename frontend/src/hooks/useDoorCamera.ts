import { useCallback, useEffect, useRef, useState } from "react";

import type { DoorDecision, FaceDetection, RecognizeResponse } from "@/types/face";

const RECOGNIZE_INTERVAL_MS = 500;
const FRAME_MAX_DIMENSION = 640;
const FRAME_QUALITY = 0.8;
const CAMERA_STORAGE_KEY = "door-camera-device";

const readStoredDevice = () => {
  try {
    return window.localStorage.getItem(CAMERA_STORAGE_KEY) ?? "";
  } catch {
    return "";
  }
};

// Camera ở cửa: webcam máy tính hoặc camera USB ngoài, chọn qua danh sách thiết bị của trình duyệt.
export const useDoorCamera = ({ recognize }: { recognize: boolean }) => {
  const videoRef = useRef<HTMLVideoElement | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const inFlightRef = useRef(false);
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [deviceId, setDeviceId] = useState<string>(readStoredDevice);
  const [isActive, setIsActive] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [faces, setFaces] = useState<FaceDetection[]>([]);
  const [decision, setDecision] = useState<DoorDecision | null>(null);

  const isSupported = typeof navigator !== "undefined" && Boolean(navigator.mediaDevices?.getUserMedia);

  const refreshDevices = useCallback(async () => {
    if (!isSupported) return;
    const all = await navigator.mediaDevices.enumerateDevices();
    setDevices(all.filter((device) => device.kind === "videoinput"));
  }, [isSupported]);

  const stop = useCallback(() => {
    streamRef.current?.getTracks().forEach((track) => track.stop());
    streamRef.current = null;
    if (videoRef.current) videoRef.current.srcObject = null;
    setIsActive(false);
    setFaces([]);
    setDecision(null);
  }, []);

  const start = useCallback(async () => {
    if (!isSupported) {
      setError("Trình duyệt chỉ cho dùng camera khi mở bằng localhost hoặc HTTPS.");
      return;
    }
    stop();
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        video: {
          deviceId: deviceId ? { exact: deviceId } : undefined,
          width: { ideal: 1280 },
          height: { ideal: 720 }
        },
        audio: false
      });
      streamRef.current = stream;
      if (videoRef.current) {
        videoRef.current.srcObject = stream;
        await videoRef.current.play().catch(() => undefined);
      }
      setError(null);
      setIsActive(true);
      // Tên thiết bị chỉ hiện sau khi người dùng đã cấp quyền camera.
      await refreshDevices();
    } catch (cause) {
      const name = cause instanceof DOMException ? cause.name : "";
      setError(
        name === "NotAllowedError"
          ? "Chưa được cấp quyền dùng camera."
          : name === "NotFoundError" || name === "OverconstrainedError"
            ? "Không tìm thấy camera đã chọn."
            : "Không mở được camera."
      );
    }
  }, [deviceId, isSupported, refreshDevices, stop]);

  const selectDevice = useCallback((id: string) => {
    setDeviceId(id);
    try {
      window.localStorage.setItem(CAMERA_STORAGE_KEY, id);
    } catch {
      // bỏ qua khi trình duyệt chặn localStorage
    }
  }, []);

  const captureFrame = useCallback((): HTMLCanvasElement | null => {
    const video = videoRef.current;
    if (!video || !video.videoWidth) return null;
    const scale = Math.min(1, FRAME_MAX_DIMENSION / Math.max(video.videoWidth, video.videoHeight));
    const canvas = canvasRef.current ?? document.createElement("canvas");
    canvasRef.current = canvas;
    canvas.width = Math.round(video.videoWidth * scale);
    canvas.height = Math.round(video.videoHeight * scale);
    canvas.getContext("2d")?.drawImage(video, 0, 0, canvas.width, canvas.height);
    return canvas;
  }, []);

  /** Ảnh JPEG (data URL) của khung hình hiện tại, dùng làm ảnh mẫu khi đăng ký thành viên. */
  const captureSample = useCallback((): string | null => {
    return captureFrame()?.toDataURL("image/jpeg", 0.9) ?? null;
  }, [captureFrame]);

  useEffect(() => {
    void refreshDevices();
  }, [refreshDevices]);

  useEffect(() => stop, [stop]);

  useEffect(() => {
    if (!isActive || !recognize) {
      setFaces([]);
      return;
    }
    const timer = window.setInterval(() => {
      if (inFlightRef.current) return;
      const canvas = captureFrame();
      if (!canvas) return;
      inFlightRef.current = true;
      canvas.toBlob(
        (blob) => {
          if (!blob) {
            inFlightRef.current = false;
            return;
          }
          fetch("/face/recognize", { method: "POST", headers: { "Content-Type": "image/jpeg" }, body: blob })
            .then(async (response) => {
              const payload = await response.json();
              if (!response.ok) throw new Error(payload?.detail ?? "Nhận diện thất bại");
              const result = payload as RecognizeResponse;
              setFaces(result.faces);
              setDecision(result.door);
              setError(null);
            })
            .catch((cause: unknown) => {
              setError(cause instanceof Error ? cause.message : "Nhận diện thất bại");
            })
            .finally(() => {
              inFlightRef.current = false;
            });
        },
        "image/jpeg",
        FRAME_QUALITY
      );
    }, RECOGNIZE_INTERVAL_MS);
    return () => window.clearInterval(timer);
  }, [captureFrame, isActive, recognize]);

  return {
    videoRef,
    devices,
    deviceId,
    selectDevice,
    isSupported,
    isActive,
    error,
    faces,
    decision,
    start,
    stop,
    captureSample
  };
};
