import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { LightBulbIcon } from "@/components/icons/LightBulbIcon";
import { MANUAL_OVERRIDE_NOTE, RELAYS, ROOMS, type IotSwitchInfo } from "@/lib/iot";
import { cn } from "@/lib/utils";
import type { IotDeviceInfo, IotDeviceState } from "@/types/iot";

interface DeviceControlCardProps {
  device: IotDeviceInfo | null;
  deviceState: IotDeviceState | null;
  pending: Record<string, boolean>;
  isStreamConnected: boolean;
  onSetLed: (roomId: string, on: boolean) => void;
  onSetRelay: (relayId: string, on: boolean) => void;
  onSetArmed: (armed: boolean) => void;
  onSilenceAlarm: () => void;
}

export function DeviceControlCard({
  device,
  deviceState,
  pending,
  isStreamConnected,
  onSetLed,
  onSetRelay,
  onSetArmed,
  onSilenceAlarm
}: DeviceControlCardProps) {
  const online = Boolean(device?.online && isStreamConnected);
  const alarms = deviceState?.alarms;
  const security = deviceState?.security;
  const outputs = deviceState?.outputs;
  const hasAlarm = Boolean(alarms?.fire || alarms?.intrusion);

  return (
    <Card className="flex flex-col">
      <CardHeader className="flex-row items-start justify-between gap-4 space-y-0">
        <div className="space-y-1.5">
          <CardTitle>Điều khiển thiết bị</CardTitle>
          <CardDescription>Lệnh chỉ hiển thị là xong khi ESP32 xác nhận.</CardDescription>
        </div>
        <DeviceStatusBadge online={online} isStreamConnected={isStreamConnected} />
      </CardHeader>

      <CardContent className="flex flex-1 flex-col gap-6">
        {hasAlarm && alarms && (
          <div role="alert" className="flex flex-col gap-3 rounded-lg border border-destructive/50 bg-destructive/10 p-4 sm:flex-row sm:items-center">
            <div className="flex-1 space-y-1">
              <p className="flex items-center gap-2 font-semibold text-destructive">
                <AlertIcon />
                {alarms.fire ? "CẢNH BÁO CHÁY" : "PHÁT HIỆN XÂM NHẬP"}
              </p>
              <p className="text-sm text-foreground">
                {alarms.fire
                  ? `${alarms.gas ? "Cảm biến khói/gas vượt ngưỡng" : "Nhiệt độ quá cao"}${alarms.gas_simulated ? " (giả lập)" : ""}. Đã ngắt quạt và đèn phụ.`
                  : alarms.door_forced
                    ? "Cửa bị mở trái phép khi đang bật An ninh. Đã bật đèn phụ."
                    : "Cảm biến chuyển động phát hiện người khi đang bật An ninh. Đã bật đèn phụ."}
              </p>
              {alarms.silenced && <p className="text-xs text-muted-foreground">Đã tắt còi, cảnh báo vẫn còn hiệu lực.</p>}
            </div>
            {!alarms.silenced && (
              <Button type="button" variant="outline" disabled={!online || pending.silence} onClick={onSilenceAlarm}>
                {pending.silence ? "Đang gửi…" : "Tắt còi"}
              </Button>
            )}
          </div>
        )}

        <section className="flex items-center justify-between gap-4 rounded-lg border p-4">
          <div className="space-y-1">
            <p className="font-semibold">Chế độ An ninh</p>
            <p className="text-sm text-muted-foreground">
              {security?.armed
                ? security.arming
                  ? "Đang kích hoạt…"
                  : "Đang bật: phát hiện chuyển động sẽ hú còi báo trộm."
                : "Đang tắt: cảm biến chuyển động dùng để tự bật đèn phòng khách."}
            </p>
          </div>
          <Toggle
            label="Chế độ An ninh"
            checked={Boolean(security?.armed)}
            disabled={!online || !security || pending.security}
            onChange={onSetArmed}
          />
        </section>

        <SwitchGroup
          title="Đèn"
          items={ROOMS}
          values={outputs?.leds}
          pendingPrefix="led"
          pending={pending}
          disabled={!online}
          onChange={onSetLed}
          showBulb
        />

        <SwitchGroup
          title="Relay"
          items={RELAYS}
          values={outputs?.relays}
          pendingPrefix="relay"
          pending={pending}
          disabled={!online}
          onChange={onSetRelay}
          note={alarms?.fire ? "Đang cảnh báo cháy: không thể bật relay." : undefined}
        />

        <p className="text-xs text-muted-foreground">{MANUAL_OVERRIDE_NOTE}</p>
      </CardContent>
    </Card>
  );
}

function DeviceStatusBadge({ online, isStreamConnected }: { online: boolean; isStreamConnected: boolean }) {
  const label = !isStreamConnected ? "Mất kết nối server" : online ? "ESP32 online" : "ESP32 offline";
  return (
    <span
      className={cn(
        "inline-flex shrink-0 items-center gap-2 rounded-full border px-3 py-1 text-xs font-medium",
        online ? "border-emerald-500/40 text-emerald-700 dark:text-emerald-300" : "border-destructive/40 text-destructive"
      )}
    >
      <span className={cn("h-2 w-2 rounded-full", online ? "bg-emerald-500" : "bg-destructive")} aria-hidden="true" />
      {label}
    </span>
  );
}

interface SwitchGroupProps {
  title: string;
  items: IotSwitchInfo[];
  values: Record<string, boolean> | undefined;
  pendingPrefix: string;
  pending: Record<string, boolean>;
  disabled: boolean;
  onChange: (id: string, on: boolean) => void;
  showBulb?: boolean;
  note?: string;
}

function SwitchGroup({ title, items, values, pendingPrefix, pending, disabled, onChange, showBulb, note }: SwitchGroupProps) {
  return (
    <section className="space-y-3">
      <div className="flex items-baseline justify-between gap-2">
        <h3 className="text-lg font-semibold">{title}</h3>
        {note && <p className="text-xs font-medium text-destructive">{note}</p>}
      </div>
      <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
        {items.map((item) => {
          const isOn = Boolean(values?.[item.id]);
          const isPending = Boolean(pending[`${pendingPrefix}:${item.id}`]);
          return (
            <div
              key={item.id}
              className={cn(
                "flex items-center gap-3 rounded-lg border p-3 transition-colors",
                isOn ? "border-amber-300/70 bg-amber-50/70 dark:border-amber-400/40 dark:bg-amber-500/10" : "bg-muted/30"
              )}
            >
              {showBulb && <LightBulbIcon isOn={isOn} className="!h-10 !w-10 shrink-0" />}
              <div className="min-w-0 flex-1">
                <p className="font-medium">{item.name}</p>
                <p className="text-xs text-muted-foreground">
                  {isPending ? "Đang gửi…" : values ? (isOn ? "Đang bật" : "Đang tắt") : "Chưa có dữ liệu"}
                </p>
                {item.automation && <p className="text-xs text-muted-foreground">{item.automation}</p>}
              </div>
              <Toggle
                label={`${title} ${item.name}`}
                checked={isOn}
                disabled={disabled || !values || isPending}
                onChange={(on) => onChange(item.id, on)}
              />
            </div>
          );
        })}
      </div>
    </section>
  );
}

interface ToggleProps {
  label: string;
  checked: boolean;
  disabled?: boolean;
  onChange: (checked: boolean) => void;
}

function Toggle({ label, checked, disabled, onChange }: ToggleProps) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      disabled={disabled}
      onClick={() => onChange(!checked)}
      className={cn(
        "relative inline-flex h-6 w-11 shrink-0 items-center rounded-full transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 disabled:cursor-not-allowed disabled:opacity-50",
        checked ? "bg-primary" : "bg-input"
      )}
    >
      <span
        className={cn(
          "inline-block h-5 w-5 rounded-full bg-background shadow transition-transform",
          checked ? "translate-x-5" : "translate-x-0.5"
        )}
      />
    </button>
  );
}

function AlertIcon() {
  return (
    <svg viewBox="0 0 24 24" className="h-5 w-5" fill="none" stroke="currentColor" strokeWidth={2} aria-hidden="true">
      <path d="M12 3 2 21h20L12 3Z" strokeLinejoin="round" />
      <path d="M12 10v5M12 18h.01" strokeLinecap="round" />
    </svg>
  );
}
