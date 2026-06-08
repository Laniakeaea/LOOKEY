#pragma once

#include <string>

namespace Keymera::functions::system::input_api {

enum class KeySide {
    none,
    left,
    right,
};

struct UnifiedInputEvent {
    std::string key;
    std::string action;
    unsigned long long timestamp_ms;
    unsigned int repeat_count;
    bool alt_pressed;
    bool ctrl_pressed;
    bool shift_pressed;
    bool win_pressed;
    unsigned int virtual_key_code;
    bool is_extended_key;
    bool left_button_pressed;
    bool right_button_pressed;
    bool middle_button_pressed;
    bool side_button_1_pressed;
    bool side_button_2_pressed;
    KeySide key_side{KeySide::none};
};

} // namespace Keymera::functions::system::input_api
