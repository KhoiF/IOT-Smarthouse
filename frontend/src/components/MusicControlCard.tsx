import { FormEvent, KeyboardEvent, useEffect, useState } from "react";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { formatDuration } from "@/lib/format";
import { cn } from "@/lib/utils";
import type { MusicPlaybackState } from "@/types/smart-home";

const SEEK_STEP_SECONDS = 10;

interface MusicControlCardProps {
  musicState: MusicPlaybackState | null;
  liveMusicPosition: number;
  musicTitle: string;
  onMusicTitleChange: (value: string) => void;
  onSubmit: (event: FormEvent<HTMLFormElement>) => void;
  suggestions: string[];
  onPlaySong: (title: string) => void;
  audioPlayerRef: React.RefObject<HTMLAudioElement>;
  currentSong: { title: string; url: string } | null;
  sortedLibrary: string[];
  audioDuration: number | null;
  onAudioDurationChange: (duration: number | null) => void;
  onSeek: (seconds: number) => void;
  onSeekBy: (deltaSeconds: number) => void;
  onTogglePlayPause: () => void;
  onAudioSeeked: () => void;
  onMusicPause: () => void;
  onMusicPlay: () => void;
  onTimeUpdate: (time: number) => void;
}

export function MusicControlCard({
  musicState,
  liveMusicPosition,
  musicTitle,
  onMusicTitleChange,
  onSubmit,
  suggestions,
  onPlaySong,
  audioPlayerRef,
  currentSong,
  sortedLibrary,
  audioDuration,
  onAudioDurationChange,
  onSeek,
  onSeekBy,
  onTogglePlayPause,
  onAudioSeeked,
  onMusicPause,
  onMusicPlay,
  onTimeUpdate
}: MusicControlCardProps) {
  const [highlighted, setHighlighted] = useState(0);
  const [showSuggestions, setShowSuggestions] = useState(false);
  // Giá trị thanh tua khi đang kéo; null = đi theo vị trí phát.
  const [dragPosition, setDragPosition] = useState<number | null>(null);

  useEffect(() => {
    setHighlighted(0);
  }, [suggestions]);

  const duration = musicState?.duration_seconds ?? audioDuration ?? 0;
  const hasSong = Boolean(musicState?.matched_song && currentSong);
  const isPlaying = musicState?.status === "playing";
  const position = Math.min(dragPosition ?? liveMusicPosition, duration || Infinity);
  const suggestionsOpen = showSuggestions && musicTitle.trim().length > 0;

  const handleKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (!suggestionsOpen || !suggestions.length) {
      return;
    }
    if (event.key === "ArrowDown") {
      event.preventDefault();
      setHighlighted((index) => (index + 1) % suggestions.length);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      setHighlighted((index) => (index - 1 + suggestions.length) % suggestions.length);
    } else if (event.key === "Enter") {
      event.preventDefault();
      onPlaySong(suggestions[highlighted] ?? suggestions[0]);
      setShowSuggestions(false);
    } else if (event.key === "Escape") {
      setShowSuggestions(false);
    }
  };

  const commitDrag = () => {
    if (dragPosition != null) {
      onSeek(dragPosition);
      setDragPosition(null);
    }
  };

  return (
    <Card className="flex flex-col">
      <CardHeader>
        <CardTitle>Phát nhạc</CardTitle>
        <CardDescription>Gõ vài chữ trong tên bài (có dấu hoặc không), rồi chọn bài trong danh sách gợi ý.</CardDescription>
      </CardHeader>
      <CardContent className="flex flex-1 flex-col gap-6">
        <form className="space-y-2" onSubmit={onSubmit} autoComplete="off">
          <Label htmlFor="song-title">Tìm bài hát</Label>
          <div className="relative">
            <Input
              id="song-title"
              value={musicTitle}
              onChange={(event) => {
                onMusicTitleChange(event.target.value);
                setShowSuggestions(true);
              }}
              onFocus={() => setShowSuggestions(true)}
              onBlur={() => window.setTimeout(() => setShowSuggestions(false), 150)}
              onKeyDown={handleKeyDown}
              placeholder="Ví dụ: noi nay, phep mau…"
              role="combobox"
              aria-expanded={suggestionsOpen}
              aria-controls="song-suggestions"
            />
            {suggestionsOpen ? (
              <ul
                id="song-suggestions"
                role="listbox"
                className="absolute z-20 mt-1 max-h-64 w-full overflow-auto rounded-md border bg-background p-1 shadow-lg"
              >
                {suggestions.length ? (
                  suggestions.map((song, index) => (
                    <li key={song} role="option" aria-selected={index === highlighted}>
                      <button
                        type="button"
                        className={cn(
                          "w-full rounded px-3 py-2 text-left text-sm",
                          index === highlighted ? "bg-accent text-accent-foreground" : "hover:bg-accent/60"
                        )}
                        onMouseDown={(event) => event.preventDefault()}
                        onMouseEnter={() => setHighlighted(index)}
                        onClick={() => {
                          onPlaySong(song);
                          setShowSuggestions(false);
                        }}
                      >
                        {song}
                      </button>
                    </li>
                  ))
                ) : (
                  <li className="px-3 py-2 text-sm text-muted-foreground">Không có bài nào khớp</li>
                )}
              </ul>
            ) : null}
          </div>
        </form>

        <div className="space-y-3">
          <div className="space-y-3 rounded-lg border bg-background p-4">
            <p className="text-sm font-semibold">
              {musicState?.matched_song
                ? `${musicState.status === "paused" ? "Đang tạm dừng" : "Đang phát"}: ${musicState.matched_song}`
                : "Chưa phát bài hát nào"}
            </p>

            <div className="space-y-1">
              <input
                type="range"
                aria-label="Tua bài hát"
                className="w-full accent-primary disabled:opacity-50"
                min={0}
                max={duration || 0}
                step={1}
                value={hasSong ? position : 0}
                disabled={!hasSong || !duration}
                onChange={(event) => setDragPosition(Number(event.target.value))}
                onPointerUp={commitDrag}
                onKeyUp={commitDrag}
                onBlur={commitDrag}
              />
              <div className="flex justify-between text-xs tabular-nums text-muted-foreground">
                <span>{formatDuration(hasSong ? position : 0)}</span>
                <span>{formatDuration(duration)}</span>
              </div>
            </div>

            <div className="flex items-center justify-center gap-2">
              <Button
                variant="outline"
                size="sm"
                disabled={!hasSong}
                onClick={() => onSeekBy(-SEEK_STEP_SECONDS)}
                aria-label={`Tua lùi ${SEEK_STEP_SECONDS} giây`}
              >
                « {SEEK_STEP_SECONDS}s
              </Button>
              <Button size="sm" className="min-w-24" disabled={!hasSong} onClick={onTogglePlayPause}>
                {isPlaying ? "Tạm dừng" : "Phát"}
              </Button>
              <Button
                variant="outline"
                size="sm"
                disabled={!hasSong}
                onClick={() => onSeekBy(SEEK_STEP_SECONDS)}
                aria-label={`Tua tới ${SEEK_STEP_SECONDS} giây`}
              >
                {SEEK_STEP_SECONDS}s »
              </Button>
            </div>

            <audio
              ref={audioPlayerRef}
              className="hidden"
              autoPlay
              playsInline
              preload="auto"
              src={currentSong?.url}
              onPause={onMusicPause}
              onPlay={onMusicPlay}
              onSeeked={onAudioSeeked}
              onLoadedMetadata={(event) => {
                const value = event.currentTarget.duration;
                onAudioDurationChange(Number.isFinite(value) && value > 0 ? value : null);
              }}
              onTimeUpdate={(event) => {
                if (dragPosition == null) {
                  onTimeUpdate(event.currentTarget.currentTime);
                }
              }}
            >
              Trình duyệt của bạn không hỗ trợ phát nhạc.
            </audio>
            {!currentSong ? (
              <p className="text-xs text-muted-foreground">Chọn một bài hát để bắt đầu phát nhạc.</p>
            ) : null}
          </div>

          <h3 className="text-lg font-semibold">Thư viện hiện có</h3>
          <div className="rounded-lg border bg-background p-2">
            {sortedLibrary.length ? (
              <ul className="grid gap-1">
                {sortedLibrary.map((song) => (
                  <li key={song}>
                    <button
                      type="button"
                      className={cn(
                        "w-full rounded px-3 py-2 text-left text-sm hover:bg-accent",
                        musicState?.matched_song === song && "font-semibold text-primary"
                      )}
                      onClick={() => onPlaySong(song)}
                    >
                      {song}
                    </button>
                  </li>
                ))}
              </ul>
            ) : (
              <p className="p-2 text-sm text-muted-foreground">Thư viện trống</p>
            )}
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
