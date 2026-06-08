#include "input-event-codec.hpp"

#include "../../../common/time/time-source.hpp"

#include <atomic>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace lookey::functions::system::input_api {

namespace {
using lookey::common::time::now_epoch_ms;

std::atomic<unsigned long long> g_event_sequence{0};

unsigned long long next_sequence() {
    return g_event_sequence.fetch_add(1) + 1;
}

const char* keyboard_action_name(KeyboardEventType type) {
    switch (type) {
        case KeyboardEventType::key_down:
            return "down";
        case KeyboardEventType::key_up:
            return "up";
        default:
            return "unknown";
    }
}

const char* mouse_key_name(MouseEventType type) {
    switch (type) {
        case MouseEventType::mouse_left_button_down:
        case MouseEventType::mouse_left_button_up:
            return "MOUSE_LEFT";
        case MouseEventType::mouse_right_button_down:
        case MouseEventType::mouse_right_button_up:
            return "MOUSE_RIGHT";
        case MouseEventType::mouse_middle_button_down:
        case MouseEventType::mouse_middle_button_up:
            return "MOUSE_MIDDLE";
        case MouseEventType::mouse_side_button_1_down:
        case MouseEventType::mouse_side_button_1_up:
            return "MOUSE_SIDE_1";
        case MouseEventType::mouse_side_button_2_down:
        case MouseEventType::mouse_side_button_2_up:
            return "MOUSE_SIDE_2";
        case MouseEventType::mouse_wheel_up:
            return "MOUSE_WHEEL_UP";
        case MouseEventType::mouse_wheel_down:
            return "MOUSE_WHEEL_DOWN";
        case MouseEventType::mouse_wheel_left:
            return "MOUSE_WHEEL_LEFT";
        case MouseEventType::mouse_wheel_right:
            return "MOUSE_WHEEL_RIGHT";
        default:
            return "unknown";
    }
}

const char* mouse_action_name(MouseEventType type) {
    switch (type) {
        case MouseEventType::mouse_left_button_down:
        case MouseEventType::mouse_right_button_down:
        case MouseEventType::mouse_middle_button_down:
        case MouseEventType::mouse_side_button_1_down:
        case MouseEventType::mouse_side_button_2_down:
            return "down";
        case MouseEventType::mouse_left_button_up:
        case MouseEventType::mouse_right_button_up:
        case MouseEventType::mouse_middle_button_up:
        case MouseEventType::mouse_side_button_1_up:
        case MouseEventType::mouse_side_button_2_up:
            return "up";
        case MouseEventType::mouse_wheel_up:
            return "scroll_up";
        case MouseEventType::mouse_wheel_down:
            return "scroll_down";
        case MouseEventType::mouse_wheel_left:
            return "scroll_left";
        case MouseEventType::mouse_wheel_right:
            return "scroll_right";
        default:
            return "unknown";
    }
}

std::string key_name_from_vk(unsigned int vk, bool is_extended_key) {
    (void)is_extended_key;

    if (vk >= 'A' && vk <= 'Z') {
        return std::string(1, static_cast<char>(vk));
    }

    if (vk >= '0' && vk <= '9') {
        return std::string(1, static_cast<char>(vk));
    }

    switch (vk) {
        case 0x1B:
            return "ESC";
        case 0x0D:
            return "ENTER";
        case 0x20:
            return "SPACE";
        case 0x21:
            return "PAGE_UP";
        case 0x22:
            return "PAGE_DOWN";
        case 0x23:
            return "END";
        case 0x24:
            return "HOME";
        case 0x09:
            return "TAB";
        case 0x08:
            return "BACKSPACE";
        case 0x14:
            return "CAPS";
        case 0x10:
        case 0xA0:
        case 0xA1:
            return "SHIFT";
        case 0x11:
        case 0xA2:
        case 0xA3:
            return "CTRL";
        case 0x12:
        case 0xA4:
        case 0xA5:
            return "ALT";
        case 0x5B:
        case 0x5C:
            return "WIN";
        case 0x2E:
            return "DELETE";
        case 0x2D:
            return "INSERT";
        case 0x13:
            return "PAUSE";
        case 0x91:
            return "SCROLL_LOCK";
        case 0x90:
            return "NUM_LOCK";
        case 0x2C:
            return "PRINT_SCREEN";
        case 0x5F:
            return "SLEEP";
        case 0xA6:
            return "BROWSER_BACK";
        case 0xA7:
            return "BROWSER_FORWARD";
        case 0xA8:
            return "BROWSER_REFRESH";
        case 0xA9:
            return "BROWSER_STOP";
        case 0xAA:
            return "BROWSER_SEARCH";
        case 0xAB:
            return "BROWSER_FAVORITES";
        case 0xAC:
            return "BROWSER_HOME";
        case 0xAD:
            return "VOLUME_MUTE";
        case 0xAE:
            return "VOLUME_DOWN";
        case 0xAF:
            return "VOLUME_UP";
        case 0xB0:
            return "MEDIA_NEXT";
        case 0xB1:
            return "MEDIA_PREV";
        case 0xB2:
            return "MEDIA_STOP";
        case 0xB3:
            return "MEDIA_PLAY_PAUSE";
        case 0xB4:
            return "LAUNCH_MAIL";
        case 0xB5:
            return "LAUNCH_MEDIA";
        case 0xB6:
            return "LAUNCH_APP_1";
        case 0xB7:
            return "LAUNCH_APP_2";
        case 0x5D:
            return "APPS";
        case 0x25:
            return "LEFT";
        case 0x26:
            return "UP";
        case 0x27:
            return "RIGHT";
        case 0x28:
            return "DOWN";
        case 0x70: return "F1";
        case 0x71: return "F2";
        case 0x72: return "F3";
        case 0x73: return "F4";
        case 0x74: return "F5";
        case 0x75: return "F6";
        case 0x76: return "F7";
        case 0x77: return "F8";
        case 0x78: return "F9";
        case 0x79: return "F10";
        case 0x7A: return "F11";
        case 0x7B: return "F12";
        case 0x7C: return "F13";
        case 0x7D: return "F14";
        case 0x7E: return "F15";
        case 0x7F: return "F16";
        case 0x80: return "F17";
        case 0x81: return "F18";
        case 0x82: return "F19";
        case 0x83: return "F20";
        case 0x84: return "F21";
        case 0x85: return "F22";
        case 0x86: return "F23";
        case 0x87: return "F24";
        case 0x60: return "NUM_0";
        case 0x61: return "NUM_1";
        case 0x62: return "NUM_2";
        case 0x63: return "NUM_3";
        case 0x64: return "NUM_4";
        case 0x65: return "NUM_5";
        case 0x66: return "NUM_6";
        case 0x67: return "NUM_7";
        case 0x68: return "NUM_8";
        case 0x69: return "NUM_9";
        case 0x6A: return "NUM_MULTIPLY";
        case 0x6B: return "NUM_ADD";
        case 0x6D: return "NUM_SUBTRACT";
        case 0x6E: return "NUM_DECIMAL";
        case 0x6F: return "NUM_DIVIDE";
        case 0xBA: return "SEMICOLON";
        case 0xBB: return "EQUAL";
        case 0xBC: return "COMMA";
        case 0xBD: return "MINUS";
        case 0xBE: return "PERIOD";
        case 0xBF: return "SLASH";
        case 0xC0: return "BACKQUOTE";
        case 0xDB: return "LBRACKET";
        case 0xDC: return "BACKSLASH";
        case 0xDD: return "RBRACKET";
        case 0xDE: return "QUOTE";
        case 0xE2: return "OEM_102";
        default:
            break;
    }

    return "UNKNOWN_KEY";
}

KeySide key_side_from_vk(unsigned int vk, bool is_extended_key) {
    switch (vk) {
        case 0xA0: // VK_LSHIFT
        case 0xA2: // VK_LCONTROL
        case 0xA4: // VK_LMENU
        case 0x5B: // VK_LWIN
            return KeySide::left;
        case 0xA1: // VK_RSHIFT
        case 0xA3: // VK_RCONTROL
        case 0xA5: // VK_RMENU
        case 0x5C: // VK_RWIN
            return KeySide::right;
        case 0x11: // VK_CONTROL
        case 0x12: // VK_MENU
            return is_extended_key ? KeySide::right : KeySide::left;
        default:
            return KeySide::none;
    }
}

const char* key_side_name(KeySide side) {
    switch (side) {
        case KeySide::left:
            return "L";
        case KeySide::right:
            return "R";
        default:
            return "N";
    }
}

} // namespace

UnifiedInputEvent to_unified_input_event(const KeyboardEvent& event) {
    UnifiedInputEvent unified_event{};
    unified_event.key = key_name_from_vk(event.virtual_key_code, event.is_extended_key);
    unified_event.action = keyboard_action_name(event.type);
    unified_event.timestamp_ms = 0;
    unified_event.repeat_count = 1;
    unified_event.alt_pressed = event.alt_pressed;
    unified_event.ctrl_pressed = event.ctrl_pressed;
    unified_event.shift_pressed = event.shift_pressed;
    unified_event.win_pressed = event.win_pressed;
    unified_event.virtual_key_code = event.virtual_key_code;
    unified_event.is_extended_key = event.is_extended_key;
    unified_event.key_side = key_side_from_vk(event.virtual_key_code, event.is_extended_key);
    return unified_event;
}

UnifiedInputEvent to_unified_input_event(const MouseEvent& event) {
    UnifiedInputEvent unified_event{};
    unified_event.key = mouse_key_name(event.type);
    unified_event.action = mouse_action_name(event.type);
    unified_event.timestamp_ms = 0;
    unified_event.repeat_count = 1;
    unified_event.alt_pressed = event.alt_pressed;
    unified_event.ctrl_pressed = event.ctrl_pressed;
    unified_event.shift_pressed = event.shift_pressed;
    unified_event.win_pressed = event.win_pressed;
    unified_event.left_button_pressed = event.left_button_pressed;
    unified_event.right_button_pressed = event.right_button_pressed;
    unified_event.middle_button_pressed = event.middle_button_pressed;
    unified_event.side_button_1_pressed = event.side_button_1_pressed;
    unified_event.side_button_2_pressed = event.side_button_2_pressed;
    unified_event.key_side = KeySide::none;
    return unified_event;
}

UnifiedInputEvent to_runtime_unified_input_event(const KeyboardEvent& event) {
    UnifiedInputEvent unified_event = to_unified_input_event(event);
    unified_event.timestamp_ms = now_epoch_ms();
    unified_event.repeat_count = 0;
    return unified_event;
}

UnifiedInputEvent to_runtime_unified_input_event(const MouseEvent& event) {
    UnifiedInputEvent unified_event = to_unified_input_event(event);
    unified_event.timestamp_ms = now_epoch_ms();
    unified_event.repeat_count = 0;
    return unified_event;
}

std::string encode_input_event(const UnifiedInputEvent& event) {
    const unsigned long long timestamp_ms = (event.timestamp_ms == 0) ? now_epoch_ms() : event.timestamp_ms;

    std::ostringstream stream;
    stream
        << "INPUT EVENT"
        << " | key=" << event.key
        << " | action=" << event.action
        << " | seq=" << next_sequence()
        << " | ts=" << timestamp_ms
        << " | repeat=" << event.repeat_count
        << " | alt=" << (event.alt_pressed ? "1" : "0")
        << " | ctrl=" << (event.ctrl_pressed ? "1" : "0")
        << " | shift=" << (event.shift_pressed ? "1" : "0")
        << " | win=" << (event.win_pressed ? "1" : "0")
        << " | vk=0x" << std::hex << std::uppercase << event.virtual_key_code << std::dec
        << " | ext=" << (event.is_extended_key ? "1" : "0")
        << " | left=" << (event.left_button_pressed ? "1" : "0")
        << " | right=" << (event.right_button_pressed ? "1" : "0")
        << " | middle=" << (event.middle_button_pressed ? "1" : "0")
        << " | side1=" << (event.side_button_1_pressed ? "1" : "0")
        << " | side2=" << (event.side_button_2_pressed ? "1" : "0")
        << " | kside=" << key_side_name(event.key_side);
    return stream.str();
}

std::string encode_keyboard_event(const KeyboardEvent& event) {
    return encode_input_event(to_unified_input_event(event));
}

std::string encode_mouse_event(const MouseEvent& event) {
    return encode_input_event(to_unified_input_event(event));
}

} // namespace lookey::functions::system::input_api
