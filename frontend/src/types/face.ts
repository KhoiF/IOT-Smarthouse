// Hợp đồng với backend /face/* và /door/* (backend/app/api/routes/face.py).

export interface FaceDetection {
  /** x, y, w, h theo tỉ lệ 0..1 của khung hình gửi lên */
  box: [number, number, number, number];
  detection_score: number;
  person_id: string | null;
  name: string | null;
  similarity: number | null;
}

export type DoorDecision =
  | { action: "none" | "stranger" }
  | { action: "confirming"; name: string; frames: number }
  | { action: "cooldown"; name: string }
  | { action: "already_open"; name: string }
  | { action: "opened"; name: string | null; disarmed?: boolean }
  | { action: "error"; name: string | null; message: string };

export interface RecognizeResponse {
  width: number;
  height: number;
  faces: FaceDetection[];
  door: DoorDecision;
}

export interface FacePerson {
  id: string;
  name: string;
  created_at: number;
  samples: number;
}

export interface DoorLogEntry {
  t: number;
  event: "face_open" | "manual_open" | "stranger" | "open_failed" | "manual_close" | "voice_close" | "close_failed";
  name: string | null;
  similarity: number | null;
  message?: string;
}
