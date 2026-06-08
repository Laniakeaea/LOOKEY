#include "input-api.hpp"

#include "input-event-codec.hpp"

#include <utility>

namespace lookey::functions::system::input_api {

// ------------------------------
// Core event bridge (fact-only)
// ------------------------------

InputApi::InputApi() {
    bind_source_handlers();
}

InputApi::~InputApi() {
    stop_input_listener();
}

void InputApi::set_event_handler(UnifiedInputEventHandler handler) {
    event_handler_ = std::move(handler);
}

void InputApi::bind_source_handlers() {
    keyboard_input_api_.set_event_handler([this](const KeyboardEvent& event) {
        on_keyboard_event(event);
    });

    mouse_input_api_.set_event_handler([this](const MouseEvent& event) {
        on_mouse_event(event);
    });
}

bool InputApi::start_input_listener() {
    if (!keyboard_input_api_.start_keyboard_listener()) {
        return false;
    }

    if (!mouse_input_api_.start_mouse_listener()) {
        keyboard_input_api_.stop_keyboard_listener();
        return false;
    }

    return true;
}

void InputApi::stop_input_listener() {
    mouse_input_api_.stop_mouse_listener();
    keyboard_input_api_.stop_keyboard_listener();
}

std::string InputApi::describe_state() const {
    return mouse_input_api_.describe_state();
}

void InputApi::on_keyboard_event(const KeyboardEvent& event) {
    if (!event_handler_) {
        return;
    }

    event_handler_(to_runtime_unified_input_event(event));
}

void InputApi::on_mouse_event(const MouseEvent& event) {
    if (!event_handler_) {
        return;
    }

    event_handler_(to_runtime_unified_input_event(event));
}

} // namespace lookey::functions::system::input_api
