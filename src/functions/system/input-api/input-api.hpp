#pragma once

#include "input-event.hpp"
#include "keyboard-input-api.hpp"
#include "mouse-input-api.hpp"

#include <functional>
#include <string>

namespace lookey::functions::system::input_api {

using UnifiedInputEventHandler = std::function<void(const UnifiedInputEvent&)>;

class InputApi {
public:
    InputApi();
    ~InputApi();

    void set_event_handler(UnifiedInputEventHandler handler);

    bool start_input_listener();
    void stop_input_listener();

    [[nodiscard]] std::string describe_state() const;

private:
    void bind_source_handlers();
    void on_keyboard_event(const KeyboardEvent& event);
    void on_mouse_event(const MouseEvent& event);

    KeyboardInputApi keyboard_input_api_;
    MouseInputApi mouse_input_api_;
    UnifiedInputEventHandler event_handler_;
};

} // namespace lookey::functions::system::input_api
