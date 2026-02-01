/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "voiceinput.h"
#include "audiocapture.h"
#include "volcenginerecognizer.h"
#include <fcitx-utils/log.h>
#include <fcitx/inputcontext.h>
#include <fcitx/userinterface.h>

FCITX_DEFINE_LOG_CATEGORY(voiceinput_logcategory, "voiceinput");

namespace fcitx {

VoiceInputManager::VoiceInputManager(AddonInstance *instance)
    : instance_(instance), state_(VoiceInputState::Idle), voiceInputAction_() {}

VoiceInputManager::~VoiceInputManager() {
    if (state_ == VoiceInputState::Recording) {
        stopRecording();
    }
}

bool VoiceInputManager::init() {
    // Initialize audio capture
    audioCapture_ = std::make_unique<AudioCapture>();
    if (!audioCapture_->init()) {
        FCITX_LOGC(voiceinput_logcategory, Error)
            << "Failed to initialize audio capture";
        return false;
    }

    // Initialize Volcengine recognizer
    recognizer_ = std::make_unique<VolcengineRecognizer>();

    // Set API credentials from configuration
    recognizer_->setAppId(*config_.appId);
    recognizer_->setToken(*config_.token);
    recognizer_->setCluster(*config_.cluster);

    if (!recognizer_->init()) {
        FCITX_LOGC(voiceinput_logcategory, Error)
            << "Failed to initialize Volcengine recognizer";
        return false;
    }

    FCITX_LOGC(voiceinput_logcategory, Info)
        << "Voice input manager initialized successfully";
    return true;
}

void VoiceInputManager::handleKeyEvent(KeyEvent &event) {
    // Only process if voice input is enabled
    if (!config_.enabled.value()) {
        return;
    }

    // Track SHIFT key states
    if (event.key().sym() == FcitxKey_Shift_L) {
        leftShiftPressed_ = !event.isRelease();
    } else if (event.key().sym() == FcitxKey_Shift_R) {
        rightShiftPressed_ = !event.isRelease();
    }

    // Handle state transitions
    handleStateTransition(event);
}

bool VoiceInputManager::isDualShiftPressed() {
    return leftShiftPressed_ && rightShiftPressed_;
}

void VoiceInputManager::handleStateTransition(KeyEvent &event) {
    switch (state_) {
    case VoiceInputState::Idle:
        // Check if we should start recording
        if (isDualShiftPressed() && !event.isRelease()) {
            startRecording();
        }
        break;

    case VoiceInputState::Recording:
        // Stop recording when either SHIFT is released
        if (event.isRelease() && (event.key().sym() == FcitxKey_Shift_L ||
                                  event.key().sym() == FcitxKey_Shift_R)) {
            stopRecording();
        }
        break;

    case VoiceInputState::Processing:
        // Ignore key events during processing
        break;

    case VoiceInputState::Result:
        // Reset to idle after result is handled
        setState(VoiceInputState::Idle);
        break;
    }
}

void VoiceInputManager::setState(VoiceInputState newState) {
    if (state_ != newState) {
        FCITX_LOGC(voiceinput_logcategory, Debug)
            << "State transition: " << static_cast<int>(state_) << " -> "
            << static_cast<int>(newState);
        state_ = newState;
    }
}

void VoiceInputManager::startRecording() {
    if (state_ != VoiceInputState::Idle) {
        return;
    }

    FCITX_LOGC(voiceinput_logcategory, Info) << "Starting voice recording";

    if (audioCapture_->startRecording()) {
        setState(VoiceInputState::Recording);
    } else {
        FCITX_LOGC(voiceinput_logcategory, Error)
            << "Failed to start recording";
    }
}

void VoiceInputManager::stopRecording() {
    if (state_ != VoiceInputState::Recording) {
        return;
    }

    FCITX_LOGC(voiceinput_logcategory, Info) << "Stopping voice recording";

    audioCapture_->stopRecording();

    // Start processing
    setState(VoiceInputState::Processing);

    // Get audio data and send to recognizer
    auto audioData = audioCapture_->getRecordedData();
    if (audioData.empty()) {
        FCITX_LOGC(voiceinput_logcategory, Debug) << "No audio data captured";
        setState(VoiceInputState::Idle);
        return;
    }

    // Send to Volcengine API
    recognizer_->recognize(
        audioData,
        [this](const std::string &result, bool isFinal) {
            if (isFinal) {
                insertResult(result);
                setState(VoiceInputState::Result);
            }
        },
        [this](const std::string &error) {
            FCITX_LOGC(voiceinput_logcategory, Error)
                << "Recognition error: " << error;
            setState(VoiceInputState::Idle);
        });
}

void VoiceInputManager::insertResult(const std::string &result) {
    if (result.empty()) {
        return;
    }

    FCITX_LOGC(voiceinput_logcategory, Info)
        << "Inserting recognition result: " << result;

    // Insert result at current cursor position in input context
    if (inputContext_) {
        inputContext_->commitString(result);
    } else {
        FCITX_LOGC(voiceinput_logcategory, Warn)
            << "No input context available for result insertion";
    }

    // Reset to idle after result is inserted
    setState(VoiceInputState::Idle);
}

} // namespace fcitx
