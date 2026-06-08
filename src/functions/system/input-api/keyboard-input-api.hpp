#pragma once

#include "windows-hook-session.hpp"

#include <functional>

namespace lookey::functions::system::input_api {

enum class KeyboardEventType {
    key_down,
    key_up,
};

struct KeyboardEvent {
    KeyboardEventType type;
    unsigned int virtual_key_code;
    bool is_extended_key;
    bool alt_pressed;
    bool ctrl_pressed;
    bool shift_pressed;
    bool win_pressed;
};

using KeyboardEventHandler = std::function<void(const KeyboardEvent&)>;

class KeyboardInputApi {
public:
    KeyboardInputApi() = default;
    ~KeyboardInputApi();

    void set_event_handler(KeyboardEventHandler handler);

    void emit_key_down(unsigned int virtual_key_code, bool is_extended_key);
    void emit_key_up(unsigned int virtual_key_code, bool is_extended_key);

    bool start_keyboard_listener();
    void stop_keyboard_listener();

private:
    void publish(KeyboardEventType event_type, unsigned int virtual_key_code, bool is_extended_key);

    WindowsHookSession hook_session_;

    KeyboardEventHandler event_handler_;
};

} // namespace lookey::functions::system::input_api
