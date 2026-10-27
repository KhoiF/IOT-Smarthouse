import { useCallback, useEffect, useRef, useState } from "react";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { useDoorCamera } from "@/hooks/useDoorCamera";
import { apiClient } from "@/lib/api-client";
import { formatClock } from "@/lib/iot";
import { cn } from "@/lib/utils";
import type { DoorDecision, DoorLogEntry, FacePerson } from "@/types/face";
import type { IotDeviceInfo, IotDeviceState } from "@/types/iot";

interface DoorCardProps {
  device: IotDeviceInfo | null;
  deviceState: IotDeviceState | null;
  showFeedback: (message: string, isError?: boolean) => void;
}

type Tab = "recognize" | "members";

const MIN_SAMPLES = 3;
const MAX_SAMPLES = 10;
const LOG_REFRESH_MS = 5000;

const DOOR_STATE_LABEL: Record<string, string> = {
  closed: "Đang đóng",
  opening: "Đang mở…",
  open: "Đang mở",
  closing: "Đang đóng lại…",
  not_closed: "Chưa đóng hẳn"
};

const LOG_LABEL: Record<DoorLogEntry["event"], string> = {
  face_open: "mở cửa (nhận diện)",
  manual_open: "mở cửa bằng nút",
  stranger: "người lạ – không mở",
  open_failed: "mở cửa thất bại",
  manual_close: "đóng cửa bằng nút",
  voice_close: "đóng cửa bằng giọng nói",
  close_failed: "đóng cửa thất bại"
};

export function DoorCard({ device, deviceState, showFeedback }: DoorCardProps) {
  const [tab, setTab] = useState<Tab>("recognize");
  const camera = useDoorCamera({ recognize: tab === "recognize" });
  const [log, setLog] = useState<DoorLogEntry[]>([]);
  const [people, setPeople] = useState<FacePerson[]>([]);
  const [pendingAction, setPendingAction] = useState<"open" | "close" | null>(null);
  // Khung video theo đúng tỉ lệ camera để khung nhận diện (tính theo tỉ lệ ảnh) không bị lệch.
  const [aspectRatio, setAspectRatio] = useState("16 / 9");

  const door = deviceState?.door;
  const online = Boolean(device?.online);

  const refreshLog = useCallback(async () => {
    try {
      setLog(await apiClient<DoorLogEntry[]>("/door/log"));
    } catch {
      // nhật ký không bắt buộc; lỗi sẽ hiện ở thao tác khác
    }
  }, []);

  const refreshPeople = useCallback(async () => {
    try {
      setPeople(await apiClient<FacePerson[]>("/face/people"));
    } catch (error) {
      if (error instanceof Error) showFeedback(error.message, true);
    }
  }, [showFeedback]);

  useEffect(() => {
    void refreshLog();
    void refreshPeople();
    const timer = window.setInterval(() => void refreshLog(), LOG_REFRESH_MS);
    return () => window.clearInterval(timer);
  }, [refreshLog, refreshPeople]);

  // Cửa vừa mở do nhận diện: cập nhật nhật ký ngay thay vì đợi lần làm mới kế tiếp.
  useEffect(() => {
    if (camera.decision?.action === "opened") void refreshLog();
  }, [camera.decision, refreshLog]);

  // Đổi camera trong lúc đang chạy thì mở lại luồng với thiết bị mới.
  const previousDevice = useRef(camera.deviceId);
  useEffect(() => {
    if (previousDevice.current !== camera.deviceId && camera.isActive) void camera.start();
    previousDevice.current = camera.deviceId;
  }, [camera]);

  const sendDoorCommand = async (action: "open" | "close") => {
    setPendingAction(action);
    try {
      const result = await apiClient<{ disarmed?: boolean }>(`/door/${action}`, { method: "POST" });
      showFeedback(
        action === "close" ? "Đã đóng cửa" : result.disarmed ? "Đã mở cửa và tắt chế độ An ninh" : "Đã mở cửa"
      );
      void refreshLog();
    } catch (error) {
      if (error instanceof Error) showFeedback(error.message, true);
    } finally {
      setPendingAction(null);
    }
  };

  const doorIsOpen = door?.state === "open" || door?.state === "opening";

  return (
    <Card className="flex flex-col">
      <CardHeader className="flex-row items-start justify-between gap-4 space-y-0">
        <div className="space-y-1.5">
          <CardTitle>Cửa ra vào</CardTitle>
          <CardDescription>Nhận diện khuôn mặt thành viên để mở cửa; mở cửa (khuôn mặt hoặc nút) sẽ tắt chế độ An ninh.</CardDescription>
        </div>
        <span
          className={cn(
            "inline-flex shrink-0 items-center rounded-full border px-3 py-1 text-xs font-medium",
            door?.held_by_fire || door?.state === "not_closed"
              ? "border-destructive/50 text-destructive"
              : door && door.state !== "closed"
                ? "border-amber-400/60 text-amber-700 dark:text-amber-300"
                : "text-muted-foreground"
          )}
        >
          {!door ? "Chưa có dữ liệu cửa" : door.held_by_fire ? "Mở thoát hiểm (cháy)" : DOOR_STATE_LABEL[door.state]}
        </span>
      </CardHeader>

      <CardContent className="flex flex-1 flex-col gap-4">
        {door && (
          <p className="text-sm text-muted-foreground">
            Cảm biến:{" "}
            {door.sensor_closed === null ? (
              "chưa lắp (trạng thái chỉ theo lệnh servo)"
            ) : (
              <span className={cn("font-medium", door.sensor_closed ? "text-foreground" : "text-destructive")}>
                {door.sensor_closed ? "cửa đóng" : "cửa hở"}
              </span>
            )}
          </p>
        )}
        {door?.state === "not_closed" && (
          <div role="alert" className="rounded-lg border border-destructive/50 bg-destructive/10 p-3 text-sm">
            <p className="font-semibold text-destructive">Cửa chưa đóng hẳn</p>
            <p>Servo đã về vị trí đóng nhưng cảm biến vẫn báo cửa hở. Kiểm tra cửa có bị kẹt hoặc vướng không.</p>
          </div>
        )}
        <div className="flex gap-2" role="tablist">
          {(["recognize", "members"] as const).map((value) => (
            <Button
              key={value}
              type="button"
              role="tab"
              aria-selected={tab === value}
              size="sm"
              variant={tab === value ? "default" : "secondary"}
              onClick={() => setTab(value)}
            >
              {value === "recognize" ? "Nhận diện" : `Thành viên (${people.length})`}
            </Button>
          ))}
        </div>

        <div className="flex flex-col gap-2 sm:flex-row sm:items-center">
          <select
            aria-label="Chọn camera"
            className="h-10 flex-1 rounded-md border bg-background px-3 text-sm"
            value={camera.deviceId}
            onChange={(event) => camera.selectDevice(event.target.value)}
          >
            <option value="">Camera mặc định</option>
            {camera.devices.map((item, index) => (
              <option key={item.deviceId || index} value={item.deviceId}>
                {item.label || `Camera ${index + 1}`}
              </option>
            ))}
          </select>
          <Button type="button" variant={camera.isActive ? "secondary" : "default"} onClick={camera.isActive ? camera.stop : () => void camera.start()}>
            {camera.isActive ? "Tắt camera" : "Bật camera"}
          </Button>
        </div>

        <div className="relative overflow-hidden rounded-lg border bg-muted/40" style={{ aspectRatio }}>
          {/* Lật ngang cả video và khung nhận diện để người đứng trước camera thấy như soi gương. */}
          <div className="absolute inset-0 -scale-x-100">
            <video
              ref={camera.videoRef}
              className="h-full w-full object-cover"
              muted
              playsInline
              onLoadedMetadata={(event) => {
                const { videoWidth, videoHeight } = event.currentTarget;
                if (videoWidth && videoHeight) setAspectRatio(`${videoWidth} / ${videoHeight}`);
              }}
            />
            {tab === "recognize" &&
              camera.faces.map((face, index) => {
                const [x, y, w, h] = face.box;
                return (
                  <div
                    key={index}
                    className={cn(
                      "absolute rounded-md border-2",
                      face.name ? "border-emerald-400" : "border-destructive"
                    )}
                    style={{ left: `${x * 100}%`, top: `${y * 100}%`, width: `${w * 100}%`, height: `${h * 100}%` }}
                  >
                    <span
                      className={cn(
                        "absolute -top-6 left-0 -scale-x-100 whitespace-nowrap rounded px-1.5 py-0.5 text-xs font-medium text-white",
                        face.name ? "bg-emerald-600" : "bg-destructive"
                      )}
                    >
                      {face.name ? `${face.name} (${Math.round((face.similarity ?? 0) * 100)}%)` : "Người lạ"}
                    </span>
                  </div>
                );
              })}
          </div>
          {!camera.isActive && (
            <p className="absolute inset-0 flex items-center justify-center p-4 text-center text-sm text-muted-foreground">
              {camera.isSupported ? "Bật camera để bắt đầu" : "Trình duyệt chỉ cho dùng camera khi mở bằng localhost hoặc HTTPS"}
            </p>
          )}
        </div>
        {camera.error && <p className="text-sm text-destructive">{camera.error}</p>}

        {tab === "recognize" ? (
          <>
            <div className="flex flex-col gap-2 sm:flex-row sm:items-center sm:justify-between">
              <p className="text-sm" aria-live="polite">
                {describeDecision(camera.decision, camera.isActive)}
              </p>
              <div className="flex gap-2">
                <Button type="button" variant="outline" disabled={!online || pendingAction !== null} onClick={() => void sendDoorCommand("open")}>
                  {pendingAction === "open" ? "Đang gửi…" : "Mở cửa"}
                </Button>
                <Button
                  type="button"
                  variant="outline"
                  disabled={!online || pendingAction !== null || !doorIsOpen || door?.held_by_fire}
                  onClick={() => void sendDoorCommand("close")}
                >
                  {pendingAction === "close" ? "Đang gửi…" : "Đóng cửa"}
                </Button>
              </div>
            </div>
            <section className="space-y-2">
              <h3 className="text-sm font-semibold">Nhật ký cửa</h3>
              {log.length ? (
                <ul className="max-h-48 space-y-1 overflow-y-auto text-sm">
                  {log.map((entry) => (
                    <li key={`${entry.t}-${entry.event}`} className="flex gap-2">
                      <span className="shrink-0 tabular-nums text-muted-foreground">{formatClock(entry.t)}</span>
                      <span className={cn(entry.event === "stranger" || entry.event.endsWith("_failed") ? "text-destructive" : "")}>
                        {entry.name ? `${entry.name} – ` : ""}
                        {LOG_LABEL[entry.event] ?? entry.event}
                        {entry.message ? ` (${entry.message})` : ""}
                      </span>
                    </li>
                  ))}
                </ul>
              ) : (
                <p className="text-sm text-muted-foreground">Chưa có sự kiện nào.</p>
              )}
            </section>
          </>
        ) : (
          <MembersPanel
            people={people}
            cameraActive={camera.isActive}
            captureSample={camera.captureSample}
            onChanged={refreshPeople}
            showFeedback={showFeedback}
          />
        )}
      </CardContent>
    </Card>
  );
}

function describeDecision(decision: DoorDecision | null, active: boolean) {
  if (!active) return "Camera đang tắt.";
  if (!decision || decision.action === "none") return "Đang chờ khuôn mặt…";
  switch (decision.action) {
    case "stranger":
      return "Không nhận ra – không mở cửa.";
    case "confirming":
      return `Nhận ra ${decision.name}, đang xác nhận…`;
    case "cooldown":
      return `${decision.name} – cửa vừa mở, chờ một lúc.`;
    case "already_open":
      return `Nhận ra ${decision.name} – cửa đang mở.`;
    case "opened":
      return `Đã mở cửa cho ${decision.name ?? "thành viên"}${decision.disarmed ? " và tắt chế độ An ninh" : ""}.`;
    case "error":
      return `Không mở được cửa: ${decision.message}`;
  }
}

interface MembersPanelProps {
  people: FacePerson[];
  cameraActive: boolean;
  captureSample: () => string | null;
  onChanged: () => Promise<void>;
  showFeedback: (message: string, isError?: boolean) => void;
}

function MembersPanel({ people, cameraActive, captureSample, onChanged, showFeedback }: MembersPanelProps) {
  const [name, setName] = useState("");
  const [samples, setSamples] = useState<string[]>([]);
  const [isSaving, setIsSaving] = useState(false);

  const addSample = () => {
    const sample = captureSample();
    if (sample) setSamples((previous) => [...previous, sample].slice(0, MAX_SAMPLES));
  };

  const save = async () => {
    setIsSaving(true);
    try {
      await apiClient("/face/people", { method: "POST", body: JSON.stringify({ name: name.trim(), images: samples }) });
      showFeedback(`Đã đăng ký khuôn mặt cho ${name.trim()}`);
      setName("");
      setSamples([]);
      await onChanged();
    } catch (error) {
      if (error instanceof Error) showFeedback(error.message, true);
    } finally {
      setIsSaving(false);
    }
  };

  const remove = async (person: FacePerson) => {
    if (!window.confirm(`Xóa khuôn mặt của ${person.name}?`)) return;
    try {
      await apiClient(`/face/people/${person.id}`, { method: "DELETE" });
      await onChanged();
    } catch (error) {
      if (error instanceof Error) showFeedback(error.message, true);
    }
  };

  return (
    <div className="space-y-4">
      <section className="space-y-2">
        <h3 className="text-sm font-semibold">Đăng ký thành viên mới</h3>
        <p className="text-xs text-muted-foreground">
          Nhìn thẳng camera, đủ sáng; chụp {MIN_SAMPLES}–{MAX_SAMPLES} ảnh với góc mặt hơi khác nhau.
        </p>
        <Input value={name} onChange={(event) => setName(event.target.value)} placeholder="Tên thành viên" maxLength={60} />
        <div className="flex flex-wrap gap-2">
          <Button type="button" variant="secondary" disabled={!cameraActive || samples.length >= MAX_SAMPLES} onClick={addSample}>
            Chụp mẫu ({samples.length}/{MAX_SAMPLES})
          </Button>
          <Button
            type="button"
            disabled={isSaving || !name.trim() || samples.length < MIN_SAMPLES}
            onClick={() => void save()}
          >
            {isSaving ? "Đang lưu…" : "Lưu thành viên"}
          </Button>
          {samples.length > 0 && (
            <Button type="button" variant="ghost" onClick={() => setSamples([])}>
              Chụp lại
            </Button>
          )}
        </div>
        {samples.length > 0 && (
          <div className="flex flex-wrap gap-2">
            {samples.map((sample, index) => (
              <img key={index} src={sample} alt={`Ảnh mẫu ${index + 1}`} className="h-14 w-20 -scale-x-100 rounded object-cover" />
            ))}
          </div>
        )}
      </section>

      <section className="space-y-2">
        <h3 className="text-sm font-semibold">Thành viên đã đăng ký</h3>
        {people.length ? (
          <ul className="space-y-2">
            {people.map((person) => (
              <li key={person.id} className="flex items-center gap-3 rounded-lg border p-2">
                <img src={`/face/people/${person.id}/thumbnail`} alt="" className="h-10 w-10 rounded object-cover" />
                <div className="min-w-0 flex-1">
                  <p className="truncate font-medium">{person.name}</p>
                  <p className="text-xs text-muted-foreground">{person.samples} ảnh mẫu</p>
                </div>
                <Button type="button" variant="ghost" size="sm" onClick={() => void remove(person)}>
                  Xóa
                </Button>
              </li>
            ))}
          </ul>
        ) : (
          <p className="text-sm text-muted-foreground">Chưa có thành viên nào.</p>
        )}
      </section>
    </div>
  );
}
