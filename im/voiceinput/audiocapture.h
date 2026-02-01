/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _VOICEINPUT_AUDIOCAPTURE_H_
#define _VOICEINPUT_AUDIOCAPTURE_H_

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fcitx {

// Audio format constants for Volcengine API
constexpr int AUDIO_SAMPLE_RATE = 16000;
constexpr int AUDIO_CHANNELS = 1;
constexpr int AUDIO_BIT_DEPTH = 16;
constexpr size_t AUDIO_BYTES_PER_SAMPLE = 2;

// Audio backend type
enum class AudioBackendType { PulseAudio, ALSA, None };

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // Initialize audio capture with auto-detection
    bool init();

    // Start recording
    bool startRecording();

    // Stop recording
    void stopRecording();

    // Check if currently recording
    bool isRecording() const { return isRecording_; }

    // Get recorded audio data
    std::vector<char> getRecordedData() const { return recordedData_; }

    // Get current audio backend
    AudioBackendType backendType() const { return backendType_; }

private:
    // Detect available audio backend
    AudioBackendType detectBackend();

    // PulseAudio specific methods
    bool initPulseAudio();
    bool startPulseAudioRecording();
    void stopPulseAudioRecording();
    void pulseAudioReadLoop();

    // ALSA specific methods
    bool initALSA();
    bool startALSARecording();
    void stopALSARecording();
    void alsaAudioReadLoop();

    AudioBackendType backendType_ = AudioBackendType::None;
    bool isRecording_ = false;
    bool stopRequested_ = false;

    // Backend-specific data
    void *pulseContext_ = nullptr;
    void *alsaDevice_ = nullptr;

    // Audio data buffer
    std::vector<char> recordedData_;

    // Reading thread for PulseAudio
    std::unique_ptr<std::thread> readThread_;
};

} // namespace fcitx

#endif // _VOICEINPUT_AUDIOCAPTURE_H_