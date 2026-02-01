/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _VOICEINPUT_VOLCENGINERECOGNIZER_H_
#define _VOICEINPUT_VOLCENGINERECOGNIZER_H_

#include "asr_client.h"
#include <curl/curl.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fcitx {

class AsrClient;

// Callback types for recognition results
using RecognitionResultCallback =
    std::function<void(const std::string &result, bool isFinal)>;
using RecognitionErrorCallback = std::function<void(const std::string &error)>;

// Adapter class to bridge AsrClient callbacks to VolcengineRecognizer callbacks
class AsrClientAdapter : public AsrCallback {
public:
    AsrClientAdapter(RecognitionResultCallback onResult,
                     RecognitionErrorCallback onError)
        : onResult_(std::move(onResult)), onError_(std::move(onError)) {}

    void on_open(AsrClient *asr_client) override {
        // Connection established, ready to send audio
    }

    void on_message(AsrClient *asr_client, std::string msg) override {
        if (onResult_) {
            // Extract recognized text from JSON response
            try {
                auto json = nlohmann::json::parse(msg);
                if (json.contains("result")) {
                    std::string result = json["result"];
                    onResult_(result, true); // WebSocket responses are final
                }
            } catch (const std::exception &e) {
                if (onError_) {
                    onError_(std::string("JSON parse error: ") + e.what());
                }
            }
        }
    }

    void on_error(AsrClient *asr_client, std::string msg) override {
        if (onError_) {
            onError_(msg);
        }
    }

    void on_close(AsrClient *asr_client) override {
        // Connection closed
    }

private:
    RecognitionResultCallback onResult_;
    RecognitionErrorCallback onError_;
};

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
    void setAccessToken(const std::string &token) { accessToken_ = token; }
    std::string accessToken() const { return accessToken_; }

    // Get cluster
    void setCluster(const std::string &cluster) { cluster_ = cluster; }
    std::string cluster() const { return cluster_; }

    // Set secret key for HMAC authentication
    void setSecretKey(const std::string &secretKey) { secretKey_ = secretKey; }
    std::string secretKey() const { return secretKey_; }

    // Set auth type: 1=Token, 2=Signature
    void setAuthType(int authType) { authType_ = authType; }
    int authType() const { return authType_; }

private:
    bool isReady_ = false;

    // API credentials
    std::string appId_;
    std::string accessToken_;
    std::string secretKey_;
    std::string cluster_;
    std::string apiEndpoint_ = "wss://openspeech.bytedance.com/api/v2/asr";
    int authType_ = 1; // 1=Token by default

    // WebSocket ASR client implementation
    std::unique_ptr<AsrClient> asrClient_;
    std::unique_ptr<AsrClientAdapter> callbackAdapter_;
};

} // namespace fcitx

#endif // _VOICEINPUT_VOLCENGINERECOGNIZER_H_
