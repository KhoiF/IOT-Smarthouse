import { useCallback, useState } from "react";

import { DeviceControlCard } from "@/components/DeviceControlCard";
import { GeminiChatCard } from "@/components/GeminiChatCard";
import { MusicControlCard } from "@/components/MusicControlCard";
import { SensorCard } from "@/components/SensorCard";
import { GEMINI_MAX_IMAGE_SIZE, useGeminiRealtime } from "@/hooks/useGeminiRealtime";
import { useIot } from "@/hooks/useIot";
import { useMusicController } from "@/hooks/useMusicController";
import { cn } from "@/lib/utils";

interface FeedbackState {
  message: string;
  isError?: boolean;
}

export default function App() {
  const [feedback, setFeedback] = useState<FeedbackState | null>(null);

  const showFeedback = useCallback((message: string, isError = false) => {
    setFeedback({ message, isError });
  }, []);

  const { device, deviceState, history, pending, isStreamConnected, setLed, setRelay, setArmed, silenceAlarm } =
    useIot({ showFeedback });

  // Trạng thái đèn đến từ luồng /ws/iot/updates sau khi ESP32 xác nhận, kể cả lệnh từ Gemini.
  const handleGeminiLightUpdate = useCallback(() => undefined, []);

  const {
    musicState,
    liveMusicPosition,
    musicTitle,
    setMusicTitle,
    audioPlayerRef,
    currentSong,
    sortedLibrary,
    handleMusicSubmit,
    handleMusicPause,
    handleMusicPlay,
    setLiveMusicPosition,
    beginMusicPlayback,
    pauseCurrentMusic,
    resumeCurrentMusic
  } = useMusicController({ showFeedback });

  const {
    geminiStatus,
    isGeminiConnected,
    chatMessages,
    chatInput,
    setChatInput,
    handleChatSubmit,
    handleChatKeyDown,
    selectedImage,
    selectedImagePreview,
    handleImageSelect,
    clearSelectedImage,
    sendSelectedImage,
    isUploadingImage,
    isRecording,
    toggleRecording,
    recordingError,
    isMicPermissionDenied,
    chatContainerRef,
    imageInputRef,
    cameraVideoRef,
    isCameraStreaming,
    toggleCameraStream,
    cameraError,
    cameraDimensions
  } = useGeminiRealtime({
    showFeedback,
    onLightUpdate: handleGeminiLightUpdate,
    beginMusicPlayback,
    pauseCurrentMusic,
    resumeCurrentMusic
  });

  return (
    <div className="min-h-screen bg-muted/30">
      <div className="container flex flex-col gap-6 py-10">
        <header className="space-y-2 text-center">
          <h1 className="text-3xl font-bold sm:text-4xl">Điều khiển Nhà Thông Minh</h1>
          <p className="text-muted-foreground">
            Giám sát cảm biến và điều khiển thiết bị trên mô hình qua ESP32.
          </p>
        </header>

        <div className="grid gap-6 lg:grid-cols-2">
          <DeviceControlCard
            device={device}
            deviceState={deviceState}
            pending={pending}
            isStreamConnected={isStreamConnected}
            onSetLed={(roomId, on) => {
              void setLed(roomId, on);
            }}
            onSetRelay={(relayId, on) => {
              void setRelay(relayId, on);
            }}
            onSetArmed={(armed) => {
              void setArmed(armed);
            }}
            onSilenceAlarm={() => {
              void silenceAlarm();
            }}
          />
          <SensorCard device={device} deviceState={deviceState} history={history} />
        </div>

        <div className="grid gap-6 lg:grid-cols-2">
          <MusicControlCard
            musicState={musicState}
            liveMusicPosition={liveMusicPosition}
            musicTitle={musicTitle}
            onMusicTitleChange={setMusicTitle}
            onSubmit={handleMusicSubmit}
            audioPlayerRef={audioPlayerRef}
            currentSong={currentSong}
            sortedLibrary={sortedLibrary}
            onMusicPause={() => {
              void handleMusicPause();
            }}
            onMusicPlay={() => {
              void handleMusicPlay();
            }}
            onTimeUpdate={setLiveMusicPosition}
          />
        </div>

        <GeminiChatCard
          geminiStatus={geminiStatus}
          isGeminiConnected={isGeminiConnected}
          isRecording={isRecording}
          toggleRecording={toggleRecording}
          isMicPermissionDenied={isMicPermissionDenied}
          recordingError={recordingError}
          isCameraStreaming={isCameraStreaming}
          toggleCameraStream={toggleCameraStream}
          cameraError={cameraError}
          handleImageSelect={handleImageSelect}
          imageInputRef={imageInputRef}
          selectedImagePreview={selectedImagePreview}
          selectedImage={selectedImage}
          clearSelectedImage={clearSelectedImage}
          sendSelectedImage={sendSelectedImage}
          isUploadingImage={isUploadingImage}
          chatMessages={chatMessages}
          chatContainerRef={chatContainerRef}
          chatInput={chatInput}
          onChatInputChange={setChatInput}
          onChatSubmit={handleChatSubmit}
          onChatKeyDown={handleChatKeyDown}
          maxImageSize={GEMINI_MAX_IMAGE_SIZE}
          cameraVideoRef={cameraVideoRef}
          cameraDimensions={cameraDimensions}
        />

        {feedback?.message && (
          <div
            className={cn(
              "rounded-lg border p-4 text-sm",
              feedback.isError
                ? "border-destructive/40 bg-destructive/10 text-destructive"
                : "border-primary/40 bg-primary/10 text-primary"
            )}
          >
            {feedback.message}
          </div>
        )}
      </div>
    </div>
  );
}
