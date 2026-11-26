// Trùng với backend/app/core/rooms.py và esp32-firmware/include/config.h.

export interface IotSwitchInfo {
  id: string;
  name: string;
  automation?: string;
}

export const ROOMS: IotSwitchInfo[] = [
  { id: "phong_khach", name: "Phòng khách", automation: "Tự bật khi tối và có chuyển động, hết chuyển động 3 giây thì tắt" },
  { id: "phong_ngu", name: "Phòng ngủ" },
  { id: "nha_bep", name: "Nhà bếp" },
  { id: "ban_cong", name: "Ban công", automation: "Tự bật khi trời tối" },
  { id: "nha_tam", name: "Nhà tắm" }
];

export const RELAYS: IotSwitchInfo[] = [
  { id: "relay1", name: "Điều hòa", automation: "Tự bật khi > 33 °C, không tự tắt" },
  { id: "relay2", name: "Quạt", automation: "Tự bật khi có khí gas hoặc báo cháy" }
];

export const MANUAL_OVERRIDE_NOTE = "Lệnh tay giữ ưu tiên 5 phút, sau đó tự động hóa chạy lại.";

export const IOT_HISTORY_LIMIT = 720;

export const formatClock = (epochSeconds: number) =>
  new Date(epochSeconds * 1000).toLocaleTimeString("vi-VN", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  });
