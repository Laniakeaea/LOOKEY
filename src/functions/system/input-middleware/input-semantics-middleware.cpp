#include "input-semantics-middleware.hpp"

#include <cctype>
#include <sstream>
#include <utility>

namespace lookey::functions::system::input_middleware {

// ------------------------------
// Public API
// ------------------------------

void InputSemanticsMiddleware::set_output_handler(SemanticInputEventHandler handler) {
    output_handler_ = std::move(handler);
}

void InputSemanticsMiddleware::set_key_repeat_behavior(const std::string& key, KeyRepeatBehavior behavior) {
    const auto code = input_api::key_code_from_string(normalize_key(key));
    const auto idx = static_cast<std::size_t>(code);
    behavior_by_code_[idx] = behavior;
    behavior_overridden_.set(idx);
}

void InputSemanticsMiddleware::clear_key_repeat_behavior(const std::string& key) {
    const auto code = input_api::key_code_from_string(normalize_key(key));
    behavior_overridden_.reset(static_cast<std::size_t>(code));
}

void InputSemanticsMiddleware::set_repeat_window_ms(unsigned long long window_ms) {
    // Keep the interval positive to avoid collapsing all repeated events into one bucket.
    repeat_window_ms_ = (window_ms == 0) ? 1 : window_ms;
}

unsigned long long InputSemanticsMiddleware::repeat_window_ms() const {
    return repeat_window_ms_;
}

void InputSemanticsMiddleware::set_exit_trigger(const ExitTrigger& trigger) {
    exit_trigger_.key = normalize_key(trigger.key);
    exit_trigger_.action = normalize_action(trigger.action);
    exit_trigger_enabled_ = true;
}

void InputSemanticsMiddleware::clear_exit_trigger() {
    exit_trigger_enabled_ = false;
}

bool InputSemanticsMiddleware::should_request_exit(const SemanticInputEvent& event) const {
    if (!exit_trigger_enabled_) {
        return false;
    }

    return normalize_key(event.base_event.key) == exit_trigger_.key
        && normalize_action(event.base_event.action) == exit_trigger_.action;
}

void InputSemanticsMiddleware::reset_state() {
    repeat_state_by_code_.fill({});
}

bool InputSemanticsMiddleware::process_event(const lookey::functions::system::input_api::UnifiedInputEvent& input_event) {
    if (!output_handler_) {
        return false;
    }

    SemanticInputEvent semantic_event{};
    semantic_event.base_event = input_event;
    semantic_event.repeat_count = 1;
    semantic_event.repeat_kind = RepeatKind::none;
    semantic_event.combo = build_combo(input_event);

    const std::string normalized_action = normalize_action(input_event.action);
    const auto key_code = input_api::key_code_from_string(normalize_key(input_event.key));
    const auto key_idx  = static_cast<std::size_t>(key_code);
    RepeatState& state = repeat_state_by_code_[key_idx];

    // Repeat behavior is only defined for key/button down bursts.
    if (normalized_action == "down") {
        const KeyRepeatBehavior behavior = resolve_repeat_behavior(key_code);

        if (behavior == KeyRepeatBehavior::allow_spam) {
            semantic_event.repeat_count = 1;
        } else {
            const bool was_down = state.is_down;
            if (behavior == KeyRepeatBehavior::ignore_spam && was_down) {
                return false;
            }

            const bool in_window = input_event.timestamp_ms >= state.last_down_ts_ms &&
                (input_event.timestamp_ms - state.last_down_ts_ms) <= repeat_window_ms_;

            if (was_down && in_window) {
                state.repeat_count += 1;
                semantic_event.repeat_kind = RepeatKind::hold;
            } else {
                state.repeat_count = 1;
                const bool tap_in_window = state.last_up_ts_ms > 0
                    && input_event.timestamp_ms >= state.last_up_ts_ms
                    && (input_event.timestamp_ms - state.last_up_ts_ms) <= repeat_window_ms_;
                state.tap_count = tap_in_window ? (state.tap_count + 1) : 1;
                if (state.tap_count > 1) {
                    semantic_event.repeat_kind = RepeatKind::tap;
                }
            }

            state.last_down_ts_ms = input_event.timestamp_ms;
            semantic_event.repeat_count = semantic_event.repeat_kind == RepeatKind::tap
                ? state.tap_count
                : state.repeat_count;

            // Suppress repeated bursts when the key policy is ignore_spam.
            if (behavior == KeyRepeatBehavior::ignore_spam && semantic_event.repeat_count > 1) {
                return false;
            }
        }

        state.is_down = true;
    } else if (normalized_action == "up") {
        state.is_down = false;
        state.last_up_ts_ms = input_event.timestamp_ms;
        state.repeat_count = 0;
    }

    semantic_event.base_event.repeat_count = semantic_event.repeat_count;
    output_handler_(semantic_event);
    return true;
}

// ------------------------------
// Helpers
// ------------------------------

std::string InputSemanticsMiddleware::normalize_key(std::string key) {
    for (char& ch : key) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return key;
}

std::string InputSemanticsMiddleware::normalize_action(std::string action) {
    for (char& ch : action) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return action;
}

bool InputSemanticsMiddleware::is_modifier_key(input_api::KeyCode code) {
    return code == input_api::KeyCode::CTRL
        || code == input_api::KeyCode::ALT
        || code == input_api::KeyCode::SHIFT
        || code == input_api::KeyCode::WIN;
}

std::string InputSemanticsMiddleware::build_combo(const lookey::functions::system::input_api::UnifiedInputEvent& event) {
    const std::string key_upper = normalize_key(event.key);
    const bool key_is_ctrl = key_upper == "CTRL";
    const bool key_is_alt = key_upper == "ALT";
    const bool key_is_shift = key_upper == "SHIFT";
    const bool key_is_win = key_upper == "WIN";

    std::ostringstream stream;
    bool has_modifier = false;

    if (event.ctrl_pressed && !key_is_ctrl) {
        stream << "CTRL+";
        has_modifier = true;
    }
    if (event.alt_pressed && !key_is_alt) {
        stream << "ALT+";
        has_modifier = true;
    }
    if (event.shift_pressed && !key_is_shift) {
        stream << "SHIFT+";
        has_modifier = true;
    }
    if (event.win_pressed && !key_is_win) {
        stream << "WIN+";
        has_modifier = true;
    }

    if (!has_modifier) {
        return "NONE";
    }

    stream << event.key << ":" << event.action;
    return stream.str();
}

KeyRepeatBehavior InputSemanticsMiddleware::resolve_repeat_behavior(input_api::KeyCode code) const {
    const auto idx = static_cast<std::size_t>(code);
    if (behavior_overridden_.test(idx)) {
        return behavior_by_code_[idx];
    }

    if (is_modifier_key(code)) {
        // Modifiers are usually held as chord gears, so suppress repeat spam by default.
        return KeyRepeatBehavior::ignore_spam;
    }

    return KeyRepeatBehavior::count;
}

} // namespace lookey::functions::system::input_middleware
