import { useEffect, useMemo, useState } from "react";

import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { HistoryChart, type ChartPoint } from "@/components/HistoryChart";
import { formatClock } from "@/lib/iot";
import { cn } from "@/lib/utils";
import type { IotDeviceInfo, IotDeviceState, IotHistoryPoint } from "@/types/iot";

interface SensorCardProps {
  device: IotDeviceInfo | null;
  deviceState: IotDeviceState | null;
  history: IotHistoryPoint[];
}

const TABLE_ROWS = 12;

export function SensorCard({ device, deviceState, history }: SensorCardProps) {
  const now = useNow();
  const sensors = deviceState?.sensors;
  const alarms = deviceState?.alarms;

  const temperaturePoints = useMemo(() => toChartPoints(history, "temperature_c"), [history]);
  const humidityPoints = useMemo(() => toChartPoints(history, "humidity_pct"), [history]);

  const lastSeen = device?.last_seen ?? null;
  const ageSeconds = lastSeen ? Math.max(0, Math.round(now / 1000 - lastSeen)) : null;

  return (
    <Card className="flex flex-col">
      <CardHeader>
        <CardTitle>Giám sát môi trường</CardTitle>
        <CardDescription>
          {lastSeen
            ? `Cập nhật lúc ${formatClock(lastSeen)} (${ageSeconds} giây trước)`
            : "Chưa nhận được dữ liệu từ ESP32"}
        </CardDescription>
      </CardHeader>
      <CardContent className="flex flex-1 flex-col gap-6">
        <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
          <StatTile
            label="Nhiệt độ"
            value={sensors?.dht_ok && sensors.temperature_c != null ? `${sensors.temperature_c.toFixed(1)} °C` : "--"}
            hint={sensors && !sensors.dht_ok ? "DHT11 lỗi đọc" : undefined}
            danger={alarms?.heat}
          />
          <StatTile
            label="Độ ẩm"
            value={sensors?.dht_ok && sensors.humidity_pct != null ? `${sensors.humidity_pct.toFixed(0)} %` : "--"}
          />
          <StatTile
            label="Khói/gas (MQ-2)"
            value={sensors ? String(sensors.mq2_raw) : "--"}
            hint={
              !sensors
                ? undefined
                : !sensors.mq2_ready
                  ? "Đang khởi động"
                  : alarms?.gas || sensors.gas_level === "danger"
                    ? "Nguy hiểm – báo cháy"
                    : sensors.gas_level === "high"
                      ? "Cao (cảnh báo)"
                      : "Bình thường"
            }
            danger={alarms?.gas}
            warning={sensors?.gas_level === "high"}
          />
          <StatTile label="Ánh sáng" value={sensors ? (sensors.is_dark ? "Tối" : "Sáng") : "--"} />
          <StatTile
            label="Chuyển động"
            value={sensors ? (sensors.motion ? "Có" : "Không") : "--"}
            hint={sensors && !sensors.pir_ready ? "PIR đang khởi động" : undefined}
            danger={alarms?.intrusion}
          />
          <StatTile
            label="Wi-Fi ESP32"
            value={deviceState && device?.online ? `${deviceState.rssi} dBm` : "--"}
            hint={device?.firmware ? `Firmware ${device.firmware}` : undefined}
          />
        </div>

        <div className="grid gap-6 sm:grid-cols-2">
          <HistoryChart title="Nhiệt độ (°C)" unit="°C" points={temperaturePoints} />
          <HistoryChart title="Độ ẩm (%)" unit="%" points={humidityPoints} decimals={0} />
        </div>

        <details className="text-sm">
          <summary className="cursor-pointer text-muted-foreground">Xem bảng số liệu gần nhất</summary>
          <table className="mt-3 w-full text-left">
            <thead className="text-xs text-muted-foreground">
              <tr>
                <th className="py-1 font-medium">Thời điểm</th>
                <th className="py-1 font-medium">Nhiệt độ</th>
                <th className="py-1 font-medium">Độ ẩm</th>
                <th className="py-1 font-medium">MQ-2</th>
              </tr>
            </thead>
            <tbody>
              {history
                .slice(-TABLE_ROWS)
                .reverse()
                .map((point) => (
                  <tr key={point.t} className="border-t">
                    <td className="py-1">{formatClock(point.t)}</td>
                    <td className="py-1">{point.temperature_c != null ? `${point.temperature_c.toFixed(1)} °C` : "--"}</td>
                    <td className="py-1">{point.humidity_pct != null ? `${point.humidity_pct.toFixed(0)} %` : "--"}</td>
                    <td className="py-1">{point.mq2_raw ?? "--"}</td>
                  </tr>
                ))}
            </tbody>
          </table>
        </details>
      </CardContent>
    </Card>
  );
}

function StatTile({
  label,
  value,
  hint,
  danger,
  warning
}: {
  label: string;
  value: string;
  hint?: string;
  danger?: boolean;
  warning?: boolean;
}) {
  return (
    <div
      className={cn(
        "rounded-lg border p-3",
        danger ? "border-destructive/60 bg-destructive/10" : warning && "border-amber-400/70 bg-amber-50/70 dark:bg-amber-500/10"
      )}
    >
      <p className="text-xs text-muted-foreground">{label}</p>
      <p className="text-xl font-semibold tabular-nums text-foreground">{value}</p>
      {hint && (
        <p
          className={cn(
            "text-xs",
            danger
              ? "font-medium text-destructive"
              : warning
                ? "font-medium text-amber-700 dark:text-amber-300"
                : "text-muted-foreground"
          )}
        >
          {warning && !danger ? "⚠ " : ""}
          {hint}
        </p>
      )}
    </div>
  );
}

function toChartPoints(history: IotHistoryPoint[], key: "temperature_c" | "humidity_pct"): ChartPoint[] {
  return history.flatMap((point) => {
    const value = point[key];
    return value == null ? [] : [{ t: point.t, value }];
  });
}

function useNow(intervalMs = 1000) {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), intervalMs);
    return () => window.clearInterval(timer);
  }, [intervalMs]);
  return now;
}
