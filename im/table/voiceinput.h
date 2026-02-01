/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _VOICEINPUT_VOICEINPUT_H_
#define _VOICEINPUT_VOICEINPUT_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx/action.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontextproperty.h>
#include <memory>
#include <string>

FCITX_DECLARE_LOG_CATEGORY(voiceinput_logcategory);

namespace fcitx {

class AudioCapture;
class VolcengineRecognizer;

// Voice input state enum
enum class VoiceInputState {
    Idle,       // Not recording
    Recording,  // Currently recording audio
    Processing, // Sending audio to API
    Result      // Recognition result available
};

// Configuration for voice input
FCITX_CONFIGURATION(
    VoiceInputConfig,
    Option<std::string> appId{this, "AppID", _("Volcengine AppID"), ""};
    Option<std::string> token{this, "Token", _("Volcengine Token"), ""};
    Option<std::string> cluster{this, "Cluster", _("Volcengine Cluster ID"),
                                ""};
    Option<bool> enabled{this, "Enabled", _("Enable Voice Input"), true};);

// Voice input manager class
class VoiceInputManager final {
public:
    VoiceInputManager(AddonInstance *instance);
    ~VoiceInputManager();

    // Initialize voice input system
    bool init();

    // Handle key events for dual SHIFT detection
    void handleKeyEvent(KeyEvent &event);

    // Start voice recording
    void startRecording();

    // Stop recording and process audio
    void stopRecording();

    // Get current state
    VoiceInputState state() const { return state_; }

    // Set the current input context for result insertion
    void setInputContext(InputContext *inputContext) {
        inputContext_ = inputContext;
    }

    // Insert recognition result at cursor position
    void insertResult(const std::string &result);

    // Configuration access
    VoiceInputConfig &config() { return config_; }
    const VoiceInputConfig &config() const { return config_; }

    // Set configuration from external source
    void setConfig(const std::string &appId, const std::string &token,
                   const std::string &cluster, bool enabled) {
        config_.appId.setValue(appId);
        config_.token.setValue(token);
        config_.cluster.setValue(cluster);
        config_.enabled.setValue(enabled);
    }

private:
    // Check if both SHIFT keys are pressed
    bool isDualShiftPressed();

    // Transition to new state
    void setState(VoiceInputState newState);

    // Handle state transitions
    void handleStateTransition(KeyEvent &event);

    AddonInstance *instance_;
    VoiceInputConfig config_;
    VoiceInputState state_ = VoiceInputState::Idle;
    InputContext *inputContext_ = nullptr;

    // SHIFT key tracking
    bool leftShiftPressed_ = false;
    bool rightShiftPressed_ = false;

    // Audio and recognition
    std::unique_ptr<AudioCapture> audioCapture_;
    std::unique_ptr<VolcengineRecognizer> recognizer_;

    // Event source for recording timeout
    std::unique_ptr<EventSource> recordingTimeout_;

    // Voice input action for UI
    SimpleAction voiceInputAction_;
};

} // namespace fcitx

#endif // _VOICEINPUT_VOICEINPUT_H_
