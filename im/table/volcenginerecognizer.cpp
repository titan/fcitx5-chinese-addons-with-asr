/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "volcenginerecognizer.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/stringutils.h>
#include <sstream>
#include <thread>

FCITX_DEFINE_LOG_CATEGORY(volcengine_recognizer_logcategory, "volcengine");

namespace fcitx {

VolcengineRecognizer::VolcengineRecognizer()
    : isReady_(false), isConnected_(false), curlHandle_(nullptr) {}

VolcengineRecognizer::~VolcengineRecognizer() { closeConnection(); }

bool VolcengineRecognizer::init() {
    // Validate API credentials
    if (appId_.empty() || token_.empty() || cluster_.empty()) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Volcengine API credentials not configured";
        FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
            << "appid: " << (appId_.empty() ? "missing" : "set")
            << ", token: " << (token_.empty() ? "missing" : "set")
            << ", cluster: " << (cluster_.empty() ? "missing" : "set");
        return false;
    }

    // Initialize libcurl
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != CURLE_OK) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Failed to initialize libcurl: " << curl_easy_strerror(res);
        return false;
    }

    isReady_ = true;

    FCITX_LOGC(volcengine_recognizer_logcategory, Info)
        << "Volcengine recognizer initialized successfully";
    return true;
}

void VolcengineRecognizer::recognize(const std::vector<char> &audioData,
                                     RecognitionResultCallback onResult,
                                     RecognitionErrorCallback onError) {
    if (!isReady_) {
        if (onError) {
            onError("Recognizer not initialized");
        }
        return;
    }

    if (audioData.empty()) {
        if (onError) {
            onError("Empty audio data");
        }
        return;
    }

    // Store callbacks
    onResult_ = onResult;
    onError_ = onError;

    // Store audio data for sending
    audioData_ = audioData;
    audioDataSent_ = 0;
    responseBuffer_.clear();
    currentResult_.clear();
    recognitionStarted_ = false;

    // Connect to WebSocket (or HTTP endpoint)
    if (!connectWebSocket()) {
        if (onError_) {
            onError_("Failed to connect to Volcengine API");
        }
        closeConnection();
        return;
    }

    // Send start event
    if (!sendStartEvent()) {
        if (onError_) {
            onError_("Failed to send start event");
        }
        closeConnection();
        return;
    }

    // Send audio data
    if (!sendAudioData(audioData)) {
        if (onError_) {
            onError_("Failed to send audio data");
        }
        closeConnection();
        return;
    }

    // Send stop event
    if (!sendStopEvent()) {
        if (onError_) {
            onError_("Failed to send stop event");
        }
        closeConnection();
        return;
    }

    FCITX_LOGC(volcengine_recognizer_logcategory, Info)
        << "Recognition request sent successfully";
}

// Static callback for receiving WebSocket data
size_t VolcengineRecognizer::websocketWriteCallback(void *ptr, size_t size,
                                                    size_t nmemb,
                                                    void *userdata) {
    VolcengineRecognizer *recognizer =
        static_cast<VolcengineRecognizer *>(userdata);
    if (!recognizer) {
        return 0;
    }

    size_t dataSize = size * nmemb;
    std::string data(static_cast<char *>(ptr), dataSize);
    recognizer->responseBuffer_ += data;

    // Parse complete JSON messages
    recognizer->parseResponse(data);

    return dataSize;
}

// Static callback for sending WebSocket data
size_t VolcengineRecognizer::websocketReadCallback(void *ptr, size_t size,
                                                   size_t nmemb,
                                                   void *userdata) {
    VolcengineRecognizer *recognizer =
        static_cast<VolcengineRecognizer *>(userdata);
    if (!recognizer) {
        return 0;
    }

    size_t remaining =
        recognizer->audioData_.size() - recognizer->audioDataSent_;
    size_t toSend = std::min(remaining, size * nmemb);

    if (toSend > 0) {
        std::memcpy(ptr, &recognizer->audioData_[recognizer->audioDataSent_],
                    toSend);
        recognizer->audioDataSent_ += toSend;
        return toSend;
    }

    // Signal end of data
    return 0;
}

bool VolcengineRecognizer::connectWebSocket() {
    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Connecting to Volcengine API: " << apiEndpoint_;

    // Create CURL handle
    curlHandle_ = curl_easy_init();
    if (!curlHandle_) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Failed to create CURL handle";
        return false;
    }

    // Set the URL (WebSocket or HTTP endpoint)
    curl_easy_setopt(curlHandle_, CURLOPT_URL, apiEndpoint_.c_str());

    // Set up callbacks for data transfer
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEFUNCTION,
                     websocketWriteCallback);
    curl_easy_setopt(curlHandle_, CURLOPT_WRITEDATA, this);

    // Set authentication header - Using Token authentication
    struct curl_slist *headers = nullptr;
    std::string authHeader = "Authorization: Bearer; " + token_;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curlHandle_, CURLOPT_HTTPHEADER, headers);

    // Enable POST for audio streaming
    curl_easy_setopt(curlHandle_, CURLOPT_POST, 1L);

    // Set POST data size (unknown for streaming)
    curl_easy_setopt(curlHandle_, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(audioData_.size()));

    // Set read callback for POST data
    curl_easy_setopt(curlHandle_, CURLOPT_READFUNCTION, websocketReadCallback);
    curl_easy_setopt(curlHandle_, CURLOPT_READDATA, this);

    // Perform the request
    CURLcode res = curl_easy_perform(curlHandle_);
    if (res != CURLE_OK) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Connection failed: " << curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curlHandle_);
        curlHandle_ = nullptr;
        return false;
    }

    // Free headers
    curl_slist_free_all(headers);

    isConnected_ = true;
    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Connected successfully";

    return true;
}

bool VolcengineRecognizer::sendStartEvent() {
    // For HTTP POST, the start event is implicit with the connection
    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Start event sent (HTTP POST established)";
    recognitionStarted_ = true;
    return true;
}

bool VolcengineRecognizer::sendAudioData(const std::vector<char> &audioData) {
    if (!isConnected_ || !curlHandle_) {
        return false;
    }

    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Audio data prepared: " << audioData.size() << " bytes";

    // Audio data is sent via the POST callback
    return true;
}

bool VolcengineRecognizer::sendStopEvent() {
    // For HTTP POST, stopping is implicit when we finish reading
    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Stop event sent (POST completed)";

    // Wait for response processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;
}

void VolcengineRecognizer::parseResponse(const std::string &json) {
    // Simple JSON parsing for Volcengine response
    // Expected format:
    // {"code":0,"message":"success","data":{"result":"识别文本"}}

    // Check for success code
    if (json.find("\"code\":0") == std::string::npos) {
        // Try to extract error message
        auto msgStart = json.find("\"message\":\"");
        if (msgStart != std::string::npos) {
            msgStart += 10; // length of ""message":""
            auto msgEnd = json.find("\"", msgStart);
            if (msgEnd != std::string::npos) {
                std::string errorMsg = json.substr(msgStart, msgEnd - msgStart);
                FCITX_LOGC(volcengine_recognizer_logcategory, Error)
                    << "API error: " << errorMsg;
            }
        }
        return;
    }

    // Extract result from data field
    auto dataStart = json.find("\"data\"");
    if (dataStart == std::string::npos) {
        // Try direct result field
        auto resultStart = json.find("\"result\":\"");
        if (resultStart != std::string::npos) {
            resultStart += 9; // length of "result":""
            auto resultEnd = json.find("\"", resultStart);
            if (resultEnd != std::string::npos) {
                std::string recognizedText =
                    json.substr(resultStart, resultEnd - resultStart);
                currentResult_ = recognizedText;

                if (onResult_ && !recognizedText.empty()) {
                    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
                        << "Recognition result: " << recognizedText;
                    onResult_(recognizedText, true); // HTTP response is final
                }
            }
        }
        return;
    }

    // Extract from data.result
    auto resultStart = json.find("\"result\":\"", dataStart);
    if (resultStart != std::string::npos) {
        resultStart += 9; // length of "result":""
        auto resultEnd = json.find("\"", resultStart);
        if (resultEnd != std::string::npos) {
            std::string recognizedText =
                json.substr(resultStart, resultEnd - resultStart);
            currentResult_ = recognizedText;

            if (onResult_ && !recognizedText.empty()) {
                FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
                    << "Recognition result: " << recognizedText;
                onResult_(recognizedText, true); // HTTP response is final
            }
        }
    }
}

void VolcengineRecognizer::handleResponse(const std::string &response) {
    // Legacy method for compatibility
    parseResponse(response);
}

void VolcengineRecognizer::closeConnection() {
    if (curlHandle_) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
            << "Closing connection";

        curl_easy_cleanup(curlHandle_);
        curlHandle_ = nullptr;
    }

    isConnected_ = false;
    recognitionStarted_ = false;
    onResult_ = nullptr;
    onError_ = nullptr;
}

} // namespace fcitx
