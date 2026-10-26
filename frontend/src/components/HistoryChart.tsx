import { useEffect, useMemo, useRef, useState, type KeyboardEvent, type PointerEvent } from "react";

import { formatClock } from "@/lib/iot";

export interface ChartPoint {
  t: number;
  value: number;
}

interface HistoryChartProps {
  title: string;
  unit: string;
  points: ChartPoint[];
  decimals?: number;
}

const HEIGHT = 140;
const PADDING = { top: 12, right: 12, bottom: 22, left: 40 };

// Biểu đồ đường một chuỗi: nét 2px, lưới mảnh, crosshair + tooltip khi rê chuột hoặc dùng phím mũi tên.
export function HistoryChart({ title, unit, points, decimals = 1 }: HistoryChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);
  const [activeIndex, setActiveIndex] = useState<number | null>(null);

  useEffect(() => {
    const element = containerRef.current;
    if (!element) return;
    const observer = new ResizeObserver(([entry]) => setWidth(entry.contentRect.width));
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  const geometry = useMemo(() => {
    if (points.length < 2 || width === 0) return null;
    const values = points.map((point) => point.value);
    let min = Math.min(...values);
    let max = Math.max(...values);
    const span = Math.max(max - min, 1);
    min -= span * 0.15;
    max += span * 0.15;

    const t0 = points[0].t;
    const t1 = points[points.length - 1].t;
    const plotWidth = width - PADDING.left - PADDING.right;
    const plotHeight = HEIGHT - PADDING.top - PADDING.bottom;
    const x = (t: number) => PADDING.left + ((t - t0) / Math.max(t1 - t0, 1)) * plotWidth;
    const y = (value: number) => PADDING.top + (1 - (value - min) / (max - min)) * plotHeight;

    const path = points.map((point, index) => `${index === 0 ? "M" : "L"}${x(point.t).toFixed(1)},${y(point.value).toFixed(1)}`).join(" ");
    const ticks = [0, 0.5, 1].map((fraction) => min + (max - min) * fraction);
    return { x, y, path, ticks };
  }, [points, width]);

  const format = (value: number) => `${value.toFixed(decimals)} ${unit}`;

  const pickIndex = (event: PointerEvent<SVGSVGElement>) => {
    if (!geometry) return;
    const rect = event.currentTarget.getBoundingClientRect();
    const pointerX = event.clientX - rect.left;
    let best = 0;
    points.forEach((point, index) => {
      if (Math.abs(geometry.x(point.t) - pointerX) < Math.abs(geometry.x(points[best].t) - pointerX)) best = index;
    });
    setActiveIndex(best);
  };

  const handleKeyDown = (event: KeyboardEvent<SVGSVGElement>) => {
    if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
    event.preventDefault();
    const step = event.key === "ArrowLeft" ? -1 : 1;
    setActiveIndex((current) => Math.min(points.length - 1, Math.max(0, (current ?? points.length - 1) + step)));
  };

  const active = activeIndex != null && geometry ? points[activeIndex] : null;

  return (
    <figure className="space-y-2">
      <figcaption className="text-sm font-medium">{title}</figcaption>
      <div ref={containerRef} className="relative">
        {geometry ? (
          <svg
            width={width}
            height={HEIGHT}
            role="img"
            aria-label={`${title}, ${points.length} điểm đo. Dùng phím mũi tên để xem từng giá trị.`}
            tabIndex={0}
            className="block touch-none focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring"
            onPointerMove={pickIndex}
            onPointerDown={pickIndex}
            onPointerLeave={() => setActiveIndex(null)}
            onBlur={() => setActiveIndex(null)}
            onKeyDown={handleKeyDown}
          >
            {geometry.ticks.map((tick) => (
              <g key={tick}>
                <line
                  x1={PADDING.left}
                  x2={width - PADDING.right}
                  y1={geometry.y(tick)}
                  y2={geometry.y(tick)}
                  stroke="hsl(var(--border))"
                  strokeWidth={1}
                />
                <text x={PADDING.left - 6} y={geometry.y(tick)} dy="0.32em" textAnchor="end" className="fill-muted-foreground text-[10px]">
                  {tick.toFixed(decimals === 0 ? 0 : 1)}
                </text>
              </g>
            ))}
            <text x={PADDING.left} y={HEIGHT - 4} className="fill-muted-foreground text-[10px]">
              {formatClock(points[0].t)}
            </text>
            <text x={width - PADDING.right} y={HEIGHT - 4} textAnchor="end" className="fill-muted-foreground text-[10px]">
              {formatClock(points[points.length - 1].t)}
            </text>
            <path d={geometry.path} fill="none" stroke="hsl(var(--primary))" strokeWidth={2} strokeLinejoin="round" strokeLinecap="round" />
            {active && (
              <g pointerEvents="none">
                <line
                  x1={geometry.x(active.t)}
                  x2={geometry.x(active.t)}
                  y1={PADDING.top}
                  y2={HEIGHT - PADDING.bottom}
                  stroke="hsl(var(--muted-foreground))"
                  strokeWidth={1}
                />
                <circle
                  cx={geometry.x(active.t)}
                  cy={geometry.y(active.value)}
                  r={4}
                  fill="hsl(var(--primary))"
                  stroke="hsl(var(--card))"
                  strokeWidth={2}
                />
              </g>
            )}
          </svg>
        ) : (
          <div className="flex h-[140px] items-center justify-center rounded-md border border-dashed text-sm text-muted-foreground">
            Chưa đủ dữ liệu để vẽ biểu đồ
          </div>
        )}
        {active && geometry && (
          <div
            className="pointer-events-none absolute top-0 z-10 rounded-md border bg-popover px-2 py-1 text-xs shadow-md"
            style={{
              left: Math.min(Math.max(geometry.x(active.t) - 48, 0), Math.max(width - 96, 0))
            }}
          >
            <p className="font-semibold text-foreground">{format(active.value)}</p>
            <p className="text-muted-foreground">{formatClock(active.t)}</p>
          </div>
        )}
      </div>
    </figure>
  );
}
