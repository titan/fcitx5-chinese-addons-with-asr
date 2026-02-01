/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "audiocapture.h"
#include <cstring>
#include <fcitx-utils/log.h>

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#endif

FCITX_DEFINE_LOG_CATEGORY(audio_capture_logcategory, "audiocapture");

namespace fcitx {

AudioCapture::AudioCapture()
    : backendType_(AudioBackendType::None), isRecording_(false) {}

AudioCapture::~AudioCapture() {
    if (isRecording_) {
        stopRecording();
    }
}

bool AudioCapture::init() {
    // Detect available audio backend
    backendType_ = detectBackend();

    if (backendType_ == AudioBackendType::None) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "No suitable audio backend found";
        return false;
    }

    FCITX_LOGC(audio_capture_logcategory, Info)
        << "Using audio backend: "
        << (backendType_ == AudioBackendType::PulseAudio ? "PulseAudio"
                                                         : "ALSA");

    // Initialize backend-specific resources
    switch (backendType_) {
    case AudioBackendType::PulseAudio:
        return initPulseAudio();
    case AudioBackendType::ALSA:
        return initALSA();
    default:
        return false;
    }
}

AudioBackendType AudioCapture::detectBackend() {
    // Try PulseAudio first
#ifdef __linux__
    if (initPulseAudio()) {
        return AudioBackendType::PulseAudio;
    }

    // Fall back to ALSA
    if (initALSA()) {
        return AudioBackendType::ALSA;
    }
#endif

    return AudioBackendType::None;
}

bool AudioCapture::initPulseAudio() {
#ifdef __linux__
    // Use PulseAudio Simple API for easier implementation
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.channels = AUDIO_CHANNELS;
    ss.rate = AUDIO_SAMPLE_RATE;

    // Create a simple connection for recording
    pa_simple *simple = nullptr;
    int error = 0;

    // Test if we can create a simple connection (for detection)
    simple =
        pa_simple_new(nullptr, "fcitx5-voiceinput", PA_STREAM_RECORD, nullptr,
                      "voice-input", &ss, nullptr, nullptr, &error);

    if (!simple) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Failed to create PulseAudio simple connection: "
            << pa_strerror(error);
        return false;
    }

    // Store the simple connection for later use
    pulseContext_ = simple;

    FCITX_LOGC(audio_capture_logcategory, Info)
        << "PulseAudio initialized successfully";
    return true;
#else
    return false;
#endif
}

bool AudioCapture::startPulseAudioRecording() {
#ifdef __linux__
    if (!pulseContext_) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "PulseAudio context is null";
        return false;
    }

    FCITX_LOGC(audio_capture_logcategory, Info)
        << "Starting PulseAudio recording";

    // Clear any existing data
    recordedData_.clear();
    stopRequested_ = false;

    // Start the read loop in a separate thread
    readThread_ =
        std::make_unique<std::thread>(&AudioCapture::pulseAudioReadLoop, this);

    isRecording_ = true;
    return true;
#else
    return false;
#endif
}

void AudioCapture::stopPulseAudioRecording() {
#ifdef __linux__
    if (isRecording_) {
        FCITX_LOGC(audio_capture_logcategory, Info)
            << "Stopping PulseAudio recording, captured "
            << recordedData_.size() << " bytes";

        // Signal the read thread to stop
        stopRequested_ = true;

        // Wait for the thread to finish
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
            readThread_.reset();
        }
    }
    isRecording_ = false;
#endif
}

void AudioCapture::pulseAudioReadLoop() {
#ifdef __linux__
    pa_simple *simple = static_cast<pa_simple *>(pulseContext_);
    const size_t bufferSize = 4096;
    char buffer[bufferSize];

    FCITX_LOGC(audio_capture_logcategory, Debug)
        << "PulseAudio read loop started";

    while (!stopRequested_) {
        int error = 0;
        // Read audio data with timeout
        ssize_t result = pa_simple_read(simple, buffer, bufferSize, &error);

        if (result < 0) {
            FCITX_LOGC(audio_capture_logcategory, Error)
                << "PulseAudio read error: " << pa_strerror(error);
            break;
        }

        if (result > 0) {
            // Add captured data to buffer
            recordedData_.insert(recordedData_.end(), buffer, buffer + result);
        }
    }

    FCITX_LOGC(audio_capture_logcategory, Debug)
        << "PulseAudio read loop finished, captured " << recordedData_.size()
        << " bytes";
#endif
}

bool AudioCapture::initALSA() {
#ifdef __linux__
    int err;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;

    // Open PCM device for capture
    err = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot open audio device: " << snd_strerror(err);
        return false;
    }

    // Allocate hardware parameters structure
    snd_pcm_hw_params_malloc(&hw_params);

    // Fill with default values
    err = snd_pcm_hw_params_any(capture_handle, hw_params);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot initialize hardware parameters: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    // Set desired parameters
    err = snd_pcm_hw_params_set_access(capture_handle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot set access type: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    // Set sample format: 16-bit little-endian
    err = snd_pcm_hw_params_set_format(capture_handle, hw_params,
                                       SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot set sample format: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    // Set sample rate: 16000 Hz
    unsigned int rate = AUDIO_SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot set sample rate: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    // Set channels: 1 (mono)
    err = snd_pcm_hw_params_set_channels(capture_handle, hw_params,
                                         AUDIO_CHANNELS);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot set channel count: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    // Apply hardware parameters
    err = snd_pcm_hw_params(capture_handle, hw_params);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot set parameters: " << snd_strerror(err);
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return false;
    }

    snd_pcm_hw_params_free(hw_params);

    // Store device handle
    alsaDevice_ = capture_handle;

    FCITX_LOGC(audio_capture_logcategory, Info)
        << "ALSA initialized successfully";
    return true;
#else
    return false;
#endif
}

bool AudioCapture::startALSARecording() {
#ifdef __linux__
    if (!alsaDevice_) {
        FCITX_LOGC(audio_capture_logcategory, Error) << "ALSA device is null";
        return false;
    }

    FCITX_LOGC(audio_capture_logcategory, Info) << "Starting ALSA recording";

    // Clear any existing data
    recordedData_.clear();
    stopRequested_ = false;

    // Start the read loop in a separate thread
    readThread_ =
        std::make_unique<std::thread>(&AudioCapture::alsaAudioReadLoop, this);

    isRecording_ = true;
    return true;
#else
    return false;
#endif
}

void AudioCapture::alsaAudioReadLoop() {
#ifdef __linux__
    snd_pcm_t *handle = static_cast<snd_pcm_t *>(alsaDevice_);
    const size_t framesPerBuffer = 1024;
    const size_t bytesPerFrame = AUDIO_BYTES_PER_SAMPLE * AUDIO_CHANNELS;
    const size_t bufferSize = framesPerBuffer * bytesPerFrame;
    char buffer[bufferSize];

    FCITX_LOGC(audio_capture_logcategory, Debug) << "ALSA read loop started";

    // Prepare the device for recording
    int err = snd_pcm_prepare(handle);
    if (err < 0) {
        FCITX_LOGC(audio_capture_logcategory, Error)
            << "Cannot prepare ALSA device: " << snd_strerror(err);
        return;
    }

    while (!stopRequested_) {
        // Read audio frames from the device
        ssize_t framesRead = snd_pcm_readi(handle, buffer, framesPerBuffer);

        if (framesRead < 0) {
            // Handle overrun or other errors by recovering
            framesRead = snd_pcm_recover(handle, framesRead, 0);
            if (framesRead < 0) {
                FCITX_LOGC(audio_capture_logcategory, Error)
                    << "ALSA read error: " << snd_strerror(framesRead);
                break;
            }
        } else if (framesRead > 0) {
            // Convert frames to bytes and add to recorded data
            size_t bytesRead = framesRead * bytesPerFrame;
            recordedData_.insert(recordedData_.end(), buffer,
                                 buffer + bytesRead);
        }
    }

    FCITX_LOGC(audio_capture_logcategory, Debug)
        << "ALSA read loop finished, captured " << recordedData_.size()
        << " bytes";
#endif
}

void AudioCapture::stopALSARecording() {
#ifdef __linux__
    if (isRecording_) {
        FCITX_LOGC(audio_capture_logcategory, Info)
            << "Stopping ALSA recording, captured " << recordedData_.size()
            << " bytes";

        // Signal the read thread to stop
        stopRequested_ = true;

        // Wait for the thread to finish
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
            readThread_.reset();
        }

        // Drop any remaining data in the buffer
        if (alsaDevice_) {
            snd_pcm_drop(static_cast<snd_pcm_t *>(alsaDevice_));
        }
    }
    isRecording_ = false;
#endif
}

bool AudioCapture::startRecording() {
    switch (backendType_) {
    case AudioBackendType::PulseAudio:
        return startPulseAudioRecording();
    case AudioBackendType::ALSA:
        return startALSARecording();
    default:
        return false;
    }
}

void AudioCapture::stopRecording() {
    switch (backendType_) {
    case AudioBackendType::PulseAudio:
        stopPulseAudioRecording();
        break;
    case AudioBackendType::ALSA:
        stopALSARecording();
        break;
    default:
        break;
    }
}

} // namespace fcitx
