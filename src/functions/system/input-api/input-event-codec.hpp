#pragma once

#include "input-event.hpp"
#include "keyboard-input-api.hpp"
#include "mouse-input-api.hpp"

#include <string>

namespace Keymera::functions::system::input_api {

// Unified event frame format:
// INPUT EVENT | key=<key_name> | action=<action_name> | seq=<number> | ts=<epoch_ms> | <extra_fields>
UnifiedInputEvent to_unified_input_event(const KeyboardEvent& event);
UnifiedInputEvent to_unified_input_event(const MouseEvent& event);

// Build runtime-ready unified events used by the core bridge.
UnifiedInputEvent to_runtime_unified_input_event(const KeyboardEvent& event);
UnifiedInputEvent to_runtime_unified_input_event(const MouseEvent& event);

std::string encode_input_event(const UnifiedInputEvent& event);

// Backward-compatible wrappers.
std::string encode_keyboard_event(const KeyboardEvent& event);
std::string encode_mouse_event(const MouseEvent& event);

} // namespace Keymera::functions::system::input_api
