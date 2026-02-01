/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "volcenginerecognizer.h"
#include <chrono>
#include <cstring>
#include <fcitx-utils/log.h>

FCITX_DEFINE_LOG_CATEGORY(volcengine_recognizer_logcategory, "volcengine");

namespace fcitx {

VolcengineRecognizer::VolcengineRecognizer() = default;

VolcengineRecognizer::~VolcengineRecognizer() = default;

bool VolcengineRecognizer::init() {
    // Validate API credentials
    if (appId_.empty() || accessToken_.empty() || cluster_.empty()) {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Volcengine API credentials not configured";
        return false;
    }

    // Create ASR client
    asrClient_ = std::make_unique<AsrClient>();

    // Configure ASR client
    asrClient_->set_appid(appId_);
    asrClient_->set_token(accessToken_);
    asrClient_->set_cluster(cluster_);

    // Set authentication type
    if (authType_ == 1) {
        asrClient_->set_auth_type(AsrClient::AuthType::TOKEN);
    } else if (authType_ == 2 && !secretKey_.empty()) {
        asrClient_->set_auth_type(AsrClient::AuthType::SIGNATURE);
        asrClient_->set_secret_key(secretKey_);
    } else {
        FCITX_LOGC(volcengine_recognizer_logcategory, Error)
            << "Invalid auth type or missing secret key for signature auth";
        return false;
    }

    // Set audio format: PCM 16kHz 16bit mono
    asrClient_->set_audio_format("raw", 1, 16000, 16);

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

    // Create callback adapter
    callbackAdapter_ = std::make_unique<AsrClientAdapter>(onResult, onError);
    asrClient_->set_callback(callbackAdapter_.get());

    // Connect to ASR service (synchronous with timeout)
    bool connected = asrClient_->sync_connect(10); // 10 second timeout
    if (!connected) {
        if (onError) {
            onError("Failed to connect to Volcengine API");
        }
        return;
    }

    FCITX_LOGC(volcengine_recognizer_logcategory, Debug)
        << "Sending audio data: " << audioData.size() << " bytes";

    // Convert vector to string and send audio data
    std::string audioStr(audioData.begin(), audioData.end());

    // Send audio data (with is_last=true to finalize recognition)
    int ret = asrClient_->send_audio(audioStr, true);

    if (ret != 0) {
        if (onError) {
            onError("Failed to send audio data");
        }
    }

    // Close the connection after sending audio
    asrClient_->close();

    FCITX_LOGC(volcengine_recognizer_logcategory, Info)
        << "Recognition request sent successfully";
}

} // namespace fcitx
