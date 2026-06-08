#pragma once

#include "../input-api/input-event.hpp"
#include "../input-api/input-key-code.hpp"

#include <array>
#include <bitset>
#include <functional>
#include <string>

namespace lookey::functions::system::input_middleware {

/// Repeat policy for per-key down-event bursts.
enum class KeyRepeatBehavior {
    count,
    ignore_spam,
    allow_spam,
};

enum class RepeatKind {
    none,
    hold,
    tap,
};

/// Middleware output event: raw input facts + semantic enrichment.
struct SemanticInputEvent {
    lookey::functions::system::input_api::UnifiedInputEvent base_event;
    unsigned int repeat_count;
    RepeatKind repeat_kind{RepeatKind::none};
    std::string combo;
};

/// Configurable semantic exit trigger (default: ESC down).
struct ExitTrigger {
    std::string key;
    std::string action;
};

using SemanticInputEventHandler = std::function<void(const SemanticInputEvent&)>;

class InputSemanticsMiddleware {
public:
    InputSemanticsMiddleware() = default;

    /// Set middleware output sink for semantic events.
    void set_output_handler(SemanticInputEventHandler handler);

    /// Override per-key repeat policy.
    void set_key_repeat_behavior(const std::string& key, KeyRepeatBehavior behavior);
    /// Remove per-key repeat policy override.
    void clear_key_repeat_behavior(const std::string& key);
    /// Set repeat detection time window in milliseconds.
    void set_repeat_window_ms(unsigned long long window_ms);
    /// Get current repeat detection window in milliseconds.
    [[nodiscard]] unsigned long long repeat_window_ms() const;
    /// Set semantic exit trigger.
    void set_exit_trigger(const ExitTrigger& trigger);
    /// Disable semantic exit trigger.
    void clear_exit_trigger();
    /// Check whether an event matches current exit trigger.
    [[nodiscard]] bool should_request_exit(const SemanticInputEvent& event) const;
    /// Reset internal repeat-tracking state.
    void reset_state();

    /// Process one raw input event and emit semantic event when not suppressed.
    bool process_event(const lookey::functions::system::input_api::UnifiedInputEvent& input_event);

private:
    static constexpr unsigned long long k_default_repeat_window_ms = 450;

    struct RepeatState {
        unsigned long long last_down_ts_ms{0};
        unsigned long long last_up_ts_ms{0};
        unsigned int repeat_count{0};
        unsigned int tap_count{0};
        bool is_down{false};
    };

    [[nodiscard]] static std::string normalize_key(std::string key);
    [[nodiscard]] static std::string normalize_action(std::string action);
    [[nodiscard]] static bool is_modifier_key(input_api::KeyCode code);
    [[nodiscard]] static std::string build_combo(const lookey::functions::system::input_api::UnifiedInputEvent& event);
    [[nodiscard]] KeyRepeatBehavior resolve_repeat_behavior(input_api::KeyCode code) const;

    static constexpr std::size_t k_key_code_count =
        static_cast<std::size_t>(input_api::KeyCode::COUNT);

    SemanticInputEventHandler output_handler_;
    std::array<RepeatState, k_key_code_count> repeat_state_by_code_{};
    std::array<KeyRepeatBehavior, k_key_code_count> behavior_by_code_{};
    std::bitset<k_key_code_count> behavior_overridden_{};
    unsigned long long repeat_window_ms_{k_default_repeat_window_ms};
    bool exit_trigger_enabled_{true};
    ExitTrigger exit_trigger_{"ESC", "down"};
};

} // namespace lookey::functions::system::input_middleware
