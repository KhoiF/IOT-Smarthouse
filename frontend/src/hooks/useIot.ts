import { useCallback, useEffect, useRef, useState } from "react";

import { apiClient } from "@/lib/api-client";
import { IOT_HISTORY_LIMIT } from "@/lib/iot";
import type {
  IotCommandResponse,
  IotDeviceInfo,
  IotDeviceState,
  IotHistoryPoint,
  IotSnapshot,
  IotUpdateMessage
} from "@/types/iot";

interface UseIotOptions {
  showFeedback: (message: string, isError?: boolean) => void;
}

export const useIot = ({ showFeedback }: UseIotOptions) => {
  const [device, setDevice] = useState<IotDeviceInfo | null>(null);
  const [deviceState, setDeviceState] = useState<IotDeviceState | null>(null);
  const [history, setHistory] = useState<IotHistoryPoint[]>([]);
  const [pending, setPending] = useState<Record<string, boolean>>({});
  const [isStreamConnected, setIsStreamConnected] = useState(false);
  const socketRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<number | null>(null);

  const applyResponse = useCallback((response: IotCommandResponse) => {
    setDevice(response.device);
    setDeviceState(response.state);
  }, []);

  const applySnapshot = useCallback(
    (snapshot: IotSnapshot) => {
      applyResponse(snapshot);
      setHistory(snapshot.history ?? []);
    },
    [applyResponse]
  );

  const refreshState = useCallback(async () => {
    try {
      applySnapshot(await apiClient<IotSnapshot>("/iot/state"));
    } catch (error) {
      if (error instanceof Error) {
        showFeedback(error.message, true);
      }
    }
  }, [applySnapshot, showFeedback]);

  // Lệnh chỉ được coi là xong khi ESP32 ACK; trạng thái hiển thị lấy từ phản hồi của backend.
  const sendCommand = useCallback(
    async (key: string, url: string, body: unknown) => {
      setPending((previous) => ({ ...previous, [key]: true }));
      try {
        applyResponse(
          await apiClient<IotCommandResponse>(url, { method: "POST", body: JSON.stringify(body) })
        );
      } catch (error) {
        if (error instanceof Error) {
          showFeedback(error.message, true);
        }
      } finally {
        setPending((previous) => {
          const next = { ...previous };
          delete next[key];
          return next;
        });
      }
    },
    [applyResponse, showFeedback]
  );

  const setLed = useCallback(
    (roomId: string, on: boolean) => sendCommand(`led:${roomId}`, `/iot/leds/${roomId}`, { on }),
    [sendCommand]
  );

  const setRelay = useCallback(
    (relayId: string, on: boolean) => sendCommand(`relay:${relayId}`, `/iot/relays/${relayId}`, { on }),
    [sendCommand]
  );

  const setArmed = useCallback(
    (armed: boolean) => sendCommand("security", "/iot/security", { armed }),
    [sendCommand]
  );

  const silenceAlarm = useCallback(() => sendCommand("silence", "/iot/alarm/silence", {}), [sendCommand]);

  useEffect(() => {
    void refreshState();
  }, [refreshState]);

  useEffect(() => {
    let isMounted = true;

    const connect = () => {
      if (!isMounted || socketRef.current) {
        return;
      }
      const protocol = window.location.protocol === "https:" ? "wss" : "ws";
      const socket = new WebSocket(`${protocol}://${window.location.host}/ws/iot/updates`);
      socketRef.current = socket;

      socket.addEventListener("open", () => setIsStreamConnected(true));

      socket.addEventListener("message", (event) => {
        try {
          const message = JSON.parse(event.data) as IotUpdateMessage;
          if (message.type === "snapshot") {
            applySnapshot(message);
          } else if (message.type === "state") {
            applyResponse(message);
            const point = message.history_point;
            if (point) {
              setHistory((previous) => [...previous, point].slice(-IOT_HISTORY_LIMIT));
            }
          }
        } catch (error) {
          console.error("Không thể phân tích dữ liệu IoT", error);
        }
      });

      socket.addEventListener("close", () => {
        if (socketRef.current === socket) {
          socketRef.current = null;
          setIsStreamConnected(false);
          if (isMounted) {
            reconnectTimer.current = window.setTimeout(connect, 3000);
          }
        }
      });

      socket.addEventListener("error", () => {
        socket.close();
      });
    };

    connect();

    return () => {
      isMounted = false;
      if (reconnectTimer.current) {
        window.clearTimeout(reconnectTimer.current);
        reconnectTimer.current = null;
      }
      socketRef.current?.close();
      socketRef.current = null;
    };
  }, [applyResponse, applySnapshot]);

  return {
    device,
    deviceState,
    history,
    pending,
    isStreamConnected,
    setLed,
    setRelay,
    setArmed,
    silenceAlarm
  };
};
