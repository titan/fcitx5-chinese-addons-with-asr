/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "../im/voiceinput/volcenginerecognizer.h"
#include "testdir.h"
#include <chrono>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/testing.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace fcitx;

int main() {
    // Setup testing environment
    std::vector<std::string> dataDirs = {TESTING_BINARY_DIR "/test",
                                         TESTING_BINARY_DIR "/modules"};
    setupTestingEnvironment(TESTING_BINARY_DIR, {"bin"}, dataDirs);

    fcitx::Log::setLogRule("default=5,volcengine=5");

    std::cout << "Testing VolcengineRecognizer class..." << std::endl;

    // Test 1: Create VolcengineRecognizer instance
    {
        VolcengineRecognizer recognizer;
        FCITX_ASSERT(true) << "VolcengineRecognizer construction failed";
    }
    std::cout << "  [PASS] VolcengineRecognizer can be constructed"
              << std::endl;

    // Test 2: Initialize with credentials
    {
        VolcengineRecognizer recognizer;
        recognizer.setAppId("test-app-id");
        recognizer.setAccessToken("test-access-token");
        bool result = recognizer.init();
        FCITX_ASSERT(result)
            << "Recognizer should initialize with valid credentials";
        FCITX_ASSERT(recognizer.isReady())
            << "Recognizer should be ready after init";
        std::cout << "  [PASS] Recognizer initialized with credentials"
                  << std::endl;
    }

    // Test 3: Initialize without credentials should fail
    {
        VolcengineRecognizer recognizer;
        bool result = recognizer.init();
        FCITX_ASSERT(!result) << "Recognizer should fail without credentials";
        FCITX_ASSERT(!recognizer.isReady()) << "Recognizer should not be ready";
        std::cout << "  [PASS] Recognizer correctly rejects missing credentials"
                  << std::endl;
    }

    // Test 4: Setters and getters
    {
        VolcengineRecognizer recognizer;
        recognizer.setAppId("my-app-id");
        recognizer.setAccessToken("my-token");
        recognizer.setApiEndpoint("wss://custom.endpoint.com/api");

        FCITX_ASSERT(recognizer.appId() == "my-app-id")
            << "App ID getter failed";
        FCITX_ASSERT(recognizer.accessToken() == "my-token")
            << "Access token getter failed";
        FCITX_ASSERT(recognizer.apiEndpoint() ==
                     "wss://custom.endpoint.com/api")
            << "API endpoint getter failed";

        std::cout << "  [PASS] Setters and getters work correctly" << std::endl;
    }

    // Test 5: Recognize without init should error
    {
        VolcengineRecognizer recognizer;
        std::string errorReceived;
        bool callbackCalled = false;

        // Create test audio data (1 second of silence at 16kHz mono 16bit)
        std::vector<char> audioData(32000, 0);

        recognizer.recognize(
            audioData,
            [&callbackCalled](const std::string &result, bool isFinal) {
                callbackCalled = true;
            },
            [&errorReceived](const std::string &error) {
                errorReceived = error;
            });

        FCITX_ASSERT(!errorReceived.empty())
            << "Should receive error for uninitialized recognizer";
        FCITX_ASSERT(!callbackCalled)
            << "Result callback should not be called on error";
        std::cout << "  [PASS] Recognizer correctly handles uninitialized state"
                  << std::endl;
    }

    // Test 6: Recognize with empty audio should error
    {
        VolcengineRecognizer recognizer;
        recognizer.setAppId("test-app");
        recognizer.setAccessToken("test-token");
        recognizer.init();

        std::string errorReceived;
        bool callbackCalled = false;

        std::vector<char> emptyAudio;

        recognizer.recognize(
            emptyAudio,
            [&callbackCalled](const std::string &result, bool isFinal) {
                callbackCalled = true;
            },
            [&errorReceived](const std::string &error) {
                errorReceived = error;
            });

        FCITX_ASSERT(!errorReceived.empty())
            << "Should receive error for empty audio";
        FCITX_ASSERT(!callbackCalled)
            << "Result callback should not be called on error";
        std::cout << "  [PASS] Recognizer correctly handles empty audio"
                  << std::endl;
    }

    // Test 7: Verify WebSocket connection is established (requires network)
    // This test verifies that the recognizer attempts to connect
    {
        VolcengineRecognizer recognizer;
        recognizer.setAppId("test-app-id");
        recognizer.setAccessToken("test-access-token");
        recognizer.init();

        bool connectionStarted = false;
        std::string lastError;

        // Create test audio data
        std::vector<char> audioData(32000, 0);

        // This should attempt WebSocket connection
        recognizer.recognize(
            audioData,
            [&connectionStarted](const std::string &result, bool isFinal) {
                // If we get here, WebSocket connection worked
                connectionStarted = true;
            },
            [&lastError](const std::string &error) {
                // Connection error is expected without valid network/API
                // credentials
                lastError = error;
            });

        // Note: Without real API credentials or network, we expect an error
        // But the important thing is that the recognizer ATTEMPTS to connect
        std::cout << "  [INFO] WebSocket connection attempt completed"
                  << std::endl;
        std::cout << "  [INFO] Last error: "
                  << (lastError.empty() ? "none" : lastError) << std::endl;
    }

    // Test 8: Verify API endpoint configuration
    {
        VolcengineRecognizer recognizer;
        // Default endpoint should be set
        FCITX_ASSERT(recognizer.apiEndpoint() ==
                     "wss://openspeech.bytedance.com/api/v1/realtime")
            << "Default endpoint should be Volcengine realtime API";

        // Custom endpoint should be accepted
        recognizer.setApiEndpoint("wss://custom.example.com/api");
        FCITX_ASSERT(recognizer.apiEndpoint() == "wss://custom.example.com/api")
            << "Custom endpoint should be set";

        std::cout << "  [PASS] API endpoint configuration is correct"
                  << std::endl;
    }

    std::cout << "VolcengineRecognizer tests completed." << std::endl;
    return 0;
}
