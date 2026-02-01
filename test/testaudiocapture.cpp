/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "../im/voiceinput/audiocapture.h"
#include "testdir.h"
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/testing.h>
#include <iostream>
#include <vector>

using namespace fcitx;

int main() {
    // Setup testing environment
    std::vector<std::string> dataDirs = {TESTING_BINARY_DIR "/test",
                                         TESTING_BINARY_DIR "/modules"};
    setupTestingEnvironment(TESTING_BINARY_DIR, {"bin"}, dataDirs);

    fcitx::Log::setLogRule("default=5,audiocapture=5");

    std::cout << "Testing AudioCapture class..." << std::endl;

    // Test 1: Create AudioCapture instance
    {
        AudioCapture capture;
        FCITX_ASSERT(true) << "AudioCapture construction failed";
    }
    std::cout << "  [PASS] AudioCapture can be constructed" << std::endl;

    // Test 2: Initialize audio capture
    {
        AudioCapture capture;
        bool result = capture.init();
        // May fail if no audio backend available, that's ok for unit test
        if (result) {
            FCITX_ASSERT(capture.backendType() != AudioBackendType::None)
                << "Backend should be set when init succeeds";
            std::cout << "  [PASS] AudioCapture initialized with backend: "
                      << (capture.backendType() == AudioBackendType::PulseAudio
                              ? "PulseAudio"
                              : "ALSA")
                      << std::endl;
        } else {
            std::cout << "  [INFO] AudioCapture init failed (no audio backend "
                         "available)"
                      << std::endl;
        }
    }

    // Test 3: Start and stop recording (if backend available)
    {
        AudioCapture capture;
        if (capture.init()) {
            bool startResult = capture.startRecording();
            if (startResult) {
                FCITX_ASSERT(capture.isRecording())
                    << "Should be recording after start";
                std::cout << "  [PASS] Recording started successfully"
                          << std::endl;

                // Small delay to capture some audio data
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Stop recording
                capture.stopRecording();
                FCITX_ASSERT(!capture.isRecording())
                    << "Should not be recording after stop";
                std::cout << "  [PASS] Recording stopped successfully"
                          << std::endl;

                // Check that we have some data (even if it's short)
                auto data = capture.getRecordedData();
                std::cout << "  [INFO] Recorded data size: " << data.size()
                          << " bytes" << std::endl;

                // For ALSA backend, verify that audio data was actually
                // captured
                if (capture.backendType() == AudioBackendType::ALSA) {
                    FCITX_ASSERT(data.size() > 0)
                        << "ALSA should have captured audio data";
                    std::cout << "  [PASS] ALSA captured " << data.size()
                              << " bytes of audio data" << std::endl;
                }
            } else {
                std::cout << "  [INFO] Could not start recording (may need "
                             "microphone)"
                          << std::endl;
            }
        } else {
            std::cout << "  [SKIP] Skipping recording test (no audio backend)"
                      << std::endl;
        }
    }

    // Test 4: Verify recording captures expected audio format
    {
        AudioCapture capture;
        if (capture.init()) {
            bool startResult = capture.startRecording();
            if (startResult) {
                // Small delay to capture some audio data
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                capture.stopRecording();

                auto data = capture.getRecordedData();
                // Audio should be captured in 16-bit mono format
                // Data size should be even (16-bit samples)
                if (data.size() > 0) {
                    FCITX_ASSERT(data.size() % 2 == 0)
                        << "Audio data size should be even for 16-bit samples";
                    std::cout << "  [PASS] Audio data format is valid (16-bit)"
                              << std::endl;
                }
            }
        }
    }

    std::cout << "AudioCapture tests completed." << std::endl;
    return 0;
}
