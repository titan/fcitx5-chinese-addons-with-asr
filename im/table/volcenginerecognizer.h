/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _VOICEINPUT_VOLCENGINERECOGNIZER_H_
#define _VOICEINPUT_VOLCENGINERECOGNIZER_H_

#include <curl/curl.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fcitx {

// Callback types for recognition results
using RecognitionResultCallback =
    std::function<void(const std::string &result, bool isFinal)>;
using RecognitionErrorCallback = std::function<void(const std::string &error)>;

class VolcengineRecognizer {
public:
    VolcengineRecognizer();
    ~VolcengineRecognizer();

    // Initialize the recognizer
    bool init();

    // Start streaming recognition
    // audioData: PCM 16kHz 16bit mono audio data
    // onResult: callback for recognition results
    // onError: callback for errors
    void recognize(const std::vector<char> &audioData,
                   RecognitionResultCallback onResult,
                   RecognitionErrorCallback onError);

    // Check if recognizer is ready
    bool isReady() const { return isReady_; }

    // Get API endpoint
    void setApiEndpoint(const std::string &endpoint) {
        apiEndpoint_ = endpoint;
    }
    std::string apiEndpoint() const { return apiEndpoint_; }

    // Get app ID
    void setAppId(const std::string &appId) { appId_ = appId; }
    std::string appId() const { return appId_; }

    // Get access token
    void setToken(const std::string &token) { token_ = token; }
    std::string token() const { return token_; }

    // Get cluster
    void setCluster(const std::string &cluster) { cluster_ = cluster; }
    std::string cluster() const { return cluster_; }

private:
    // WebSocket callback for libcurl
    static size_t websocketWriteCallback(void *ptr, size_t size, size_t nmemb,
                                         void *userdata);
    static size_t websocketReadCallback(void *ptr, size_t size, size_t nmemb,
                                        void *userdata);

    // Connect to Volcengine WebSocket
    bool connectWebSocket();

    // Send audio data through WebSocket
    bool sendAudioData(const std::vector<char> &audioData);

    // Send start event to begin recognition session
    bool sendStartEvent();

    // Send stop event to end recognition session
    bool sendStopEvent();

    // Handle recognition response
    void handleResponse(const std::string &response);

    // Close WebSocket connection
    void closeConnection();

    // Parse JSON response and extract result
    void parseResponse(const std::string &json);

    bool isReady_ = false;
    bool isConnected_ = false;

    // API credentials
    std::string appId_;
    std::string token_;
    std::string cluster_;
    std::string apiEndpoint_ = "wss://openspeech.bytedance.com/api/v2/asr";

    // WebSocket handle
    CURL *curlHandle_ = nullptr;

    // Response buffer for receiving WebSocket data
    std::string responseBuffer_;

    // Audio data being sent
    std::vector<char> audioData_;
    size_t audioDataSent_ = 0;

    // Callbacks
    RecognitionResultCallback onResult_;
    RecognitionErrorCallback onError_;

    // Current recognition state
    std::string currentResult_;
    bool recognitionStarted_ = false;
};

} // namespace fcitx

#endif // _VOICEINPUT_VOLCENGINERECOGNIZER_H_
