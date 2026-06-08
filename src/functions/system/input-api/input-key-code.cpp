#include "input-key-code.hpp"

#include <unordered_map>

namespace Keymera::functions::system::input_api {

namespace {

const std::unordered_map<std::string, KeyCode>& build_key_code_table() {
    static const std::unordered_map<std::string, KeyCode> table{
        // Letters
        {"A", KeyCode::A}, {"B", KeyCode::B}, {"C", KeyCode::C}, {"D", KeyCode::D},
        {"E", KeyCode::E}, {"F", KeyCode::F}, {"G", KeyCode::G}, {"H", KeyCode::H},
        {"I", KeyCode::I}, {"J", KeyCode::J}, {"K", KeyCode::K}, {"L", KeyCode::L},
        {"M", KeyCode::M}, {"N", KeyCode::N}, {"O", KeyCode::O}, {"P", KeyCode::P},
        {"Q", KeyCode::Q}, {"R", KeyCode::R}, {"S", KeyCode::S}, {"T", KeyCode::T},
        {"U", KeyCode::U}, {"V", KeyCode::V}, {"W", KeyCode::W}, {"X", KeyCode::X},
        {"Y", KeyCode::Y}, {"Z", KeyCode::Z},

        // Digits
        {"0", KeyCode::k0}, {"1", KeyCode::k1}, {"2", KeyCode::k2}, {"3", KeyCode::k3},
        {"4", KeyCode::k4}, {"5", KeyCode::k5}, {"6", KeyCode::k6}, {"7", KeyCode::k7},
        {"8", KeyCode::k8}, {"9", KeyCode::k9},

        // Function keys
        {"F1",  KeyCode::F1},  {"F2",  KeyCode::F2},  {"F3",  KeyCode::F3},
        {"F4",  KeyCode::F4},  {"F5",  KeyCode::F5},  {"F6",  KeyCode::F6},
        {"F7",  KeyCode::F7},  {"F8",  KeyCode::F8},  {"F9",  KeyCode::F9},
        {"F10", KeyCode::F10}, {"F11", KeyCode::F11}, {"F12", KeyCode::F12},
        {"F13", KeyCode::F13}, {"F14", KeyCode::F14}, {"F15", KeyCode::F15},
        {"F16", KeyCode::F16}, {"F17", KeyCode::F17}, {"F18", KeyCode::F18},
        {"F19", KeyCode::F19}, {"F20", KeyCode::F20}, {"F21", KeyCode::F21},
        {"F22", KeyCode::F22}, {"F23", KeyCode::F23}, {"F24", KeyCode::F24},

        // Navigation / editing
        {"ESC",          KeyCode::ESC},
        {"ENTER",        KeyCode::ENTER},
        {"SPACE",        KeyCode::SPACE},
        {"TAB",          KeyCode::TAB},
        {"BACKSPACE",    KeyCode::BACKSPACE},
        {"CAPS",         KeyCode::CAPS},
        {"DELETE",       KeyCode::KEY_DELETE},
        {"INSERT",       KeyCode::INSERT},
        {"HOME",         KeyCode::HOME},
        {"END",          KeyCode::END},
        {"PAGE_UP",      KeyCode::PAGE_UP},
        {"PAGE_DOWN",    KeyCode::PAGE_DOWN},
        {"PRINT_SCREEN", KeyCode::PRINT_SCREEN},
        {"SCROLL_LOCK",  KeyCode::SCROLL_LOCK},
        {"PAUSE",        KeyCode::PAUSE},
        {"NUM_LOCK",     KeyCode::NUM_LOCK},
        {"SHIFT",        KeyCode::SHIFT},
        {"CTRL",         KeyCode::CTRL},
        {"ALT",          KeyCode::ALT},
        {"WIN",          KeyCode::WIN},
        {"UP",           KeyCode::UP},
        {"DOWN",         KeyCode::DOWN},
        {"LEFT",         KeyCode::LEFT},
        {"RIGHT",        KeyCode::RIGHT},
        {"APPS",         KeyCode::APPS},
        {"SLEEP",        KeyCode::SLEEP},

        // Punctuation
        {"SEMICOLON",  KeyCode::SEMICOLON},
        {"EQUAL",      KeyCode::EQUAL},
        {"COMMA",      KeyCode::COMMA},
        {"MINUS",      KeyCode::MINUS},
        {"PERIOD",     KeyCode::PERIOD},
        {"SLASH",      KeyCode::SLASH},
        {"BACKQUOTE",  KeyCode::BACKQUOTE},
        {"LBRACKET",   KeyCode::LBRACKET},
        {"RBRACKET",   KeyCode::RBRACKET},
        {"BACKSLASH",  KeyCode::BACKSLASH},
        {"OEM_102",    KeyCode::OEM_102},
        {"QUOTE",      KeyCode::QUOTE},

        // Numpad
        {"NUM_0",        KeyCode::NUM_0},
        {"NUM_1",        KeyCode::NUM_1},
        {"NUM_2",        KeyCode::NUM_2},
        {"NUM_3",        KeyCode::NUM_3},
        {"NUM_4",        KeyCode::NUM_4},
        {"NUM_5",        KeyCode::NUM_5},
        {"NUM_6",        KeyCode::NUM_6},
        {"NUM_7",        KeyCode::NUM_7},
        {"NUM_8",        KeyCode::NUM_8},
        {"NUM_9",        KeyCode::NUM_9},
        {"NUM_MULTIPLY", KeyCode::NUM_MULTIPLY},
        {"NUM_ADD",      KeyCode::NUM_ADD},
        {"NUM_SUBTRACT", KeyCode::NUM_SUBTRACT},
        {"NUM_DECIMAL",  KeyCode::NUM_DECIMAL},
        {"NUM_DIVIDE",   KeyCode::NUM_DIVIDE},

        // Media / Browser / Launch
        {"VOLUME_MUTE",       KeyCode::VOLUME_MUTE},
        {"VOLUME_DOWN",       KeyCode::VOLUME_DOWN},
        {"VOLUME_UP",         KeyCode::VOLUME_UP},
        {"MEDIA_NEXT",        KeyCode::MEDIA_NEXT},
        {"MEDIA_PREV",        KeyCode::MEDIA_PREV},
        {"MEDIA_STOP",        KeyCode::MEDIA_STOP},
        {"MEDIA_PLAY_PAUSE",  KeyCode::MEDIA_PLAY_PAUSE},
        {"BROWSER_BACK",      KeyCode::BROWSER_BACK},
        {"BROWSER_FORWARD",   KeyCode::BROWSER_FORWARD},
        {"BROWSER_REFRESH",   KeyCode::BROWSER_REFRESH},
        {"BROWSER_STOP",      KeyCode::BROWSER_STOP},
        {"BROWSER_SEARCH",    KeyCode::BROWSER_SEARCH},
        {"BROWSER_FAVORITES", KeyCode::BROWSER_FAVORITES},
        {"BROWSER_HOME",      KeyCode::BROWSER_HOME},
        {"LAUNCH_MAIL",       KeyCode::LAUNCH_MAIL},
        {"LAUNCH_MEDIA",      KeyCode::LAUNCH_MEDIA},
        {"LAUNCH_APP_1",      KeyCode::LAUNCH_APP_1},
        {"LAUNCH_APP_2",      KeyCode::LAUNCH_APP_2},

        // Mouse
        {"MOUSE_LEFT",        KeyCode::MOUSE_LEFT},
        {"MOUSE_RIGHT",       KeyCode::MOUSE_RIGHT},
        {"MOUSE_MIDDLE",      KeyCode::MOUSE_MIDDLE},
        {"MOUSE_SIDE_1",      KeyCode::MOUSE_SIDE_1},
        {"MOUSE_SIDE_2",      KeyCode::MOUSE_SIDE_2},
        {"MOUSE_WHEEL_UP",    KeyCode::MOUSE_WHEEL_UP},
        {"MOUSE_WHEEL_DOWN",  KeyCode::MOUSE_WHEEL_DOWN},
        {"MOUSE_WHEEL_LEFT",  KeyCode::MOUSE_WHEEL_LEFT},
        {"MOUSE_WHEEL_RIGHT", KeyCode::MOUSE_WHEEL_RIGHT},

        // Fallback from codec
        {"UNKNOWN_KEY", KeyCode::UNKNOWN},
    };
    return table;
}

} // namespace

KeyCode key_code_from_string(const std::string& key_name_upper) {
    const auto& table = build_key_code_table();
    const auto it = table.find(key_name_upper);
    return it != table.end() ? it->second : KeyCode::UNKNOWN;
}

InputAction input_action_from_string(const std::string& action_lower) {
    if (action_lower == "down")         return InputAction::down;
    if (action_lower == "up")           return InputAction::up;
    if (action_lower == "scroll_up")    return InputAction::scroll_up;
    if (action_lower == "scroll_down")  return InputAction::scroll_down;
    if (action_lower == "scroll_left")  return InputAction::scroll_left;
    if (action_lower == "scroll_right") return InputAction::scroll_right;
    return InputAction::unknown;
}

} // namespace Keymera::functions::system::input_api
