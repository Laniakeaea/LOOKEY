#include "mouse-input-api.hpp"

#include <iostream>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Keymera::functions::system::input_api {

namespace {
struct ModifierState {
    bool alt_pressed;
    bool ctrl_pressed;
    bool shift_pressed;
    bool win_pressed;
};
} // namespace

#if defined(_WIN32)
namespace {
MouseInputApi* g_windows_mouse_api_instance = nullptr;

ModifierState current_modifier_state() {
    return ModifierState{
        (GetAsyncKeyState(VK_MENU) & 0x8000) != 0,
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0,
        ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0)};
}

LRESULT CALLBACK windows_low_level_mouse_proc(int code, WPARAM w_param, LPARAM l_param) {
    if (code < 0 || g_windows_mouse_api_instance == nullptr) {
        return CallNextHookEx(nullptr, code, w_param, l_param);
    }

    auto* api = g_windows_mouse_api_instance;

    switch (w_param) {
        case WM_LBUTTONDOWN:
            api->emit_mouse_left_button_down();
            break;
        case WM_LBUTTONUP:
            api->emit_mouse_left_button_up();
            break;
        case WM_RBUTTONDOWN:
            api->emit_mouse_right_button_down();
            break;
        case WM_RBUTTONUP:
            api->emit_mouse_right_button_up();
            break;
        case WM_MBUTTONDOWN:
            api->emit_mouse_middle_button_down();
            break;
        case WM_MBUTTONUP:
            api->emit_mouse_middle_button_up();
            break;
        case WM_XBUTTONDOWN: {
            const auto* mouse_struct = reinterpret_cast<const MSLLHOOKSTRUCT*>(l_param);
            const unsigned short button = HIWORD(mouse_struct->mouseData);
            if (button == XBUTTON1) {
                api->emit_mouse_side_button_1_down();
            } else if (button == XBUTTON2) {
                api->emit_mouse_side_button_2_down();
            }
            break;
        }
        case WM_XBUTTONUP: {
            const auto* mouse_struct = reinterpret_cast<const MSLLHOOKSTRUCT*>(l_param);
            const unsigned short button = HIWORD(mouse_struct->mouseData);
            if (button == XBUTTON1) {
                api->emit_mouse_side_button_1_up();
            } else if (button == XBUTTON2) {
                api->emit_mouse_side_button_2_up();
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            const auto* mouse_struct = reinterpret_cast<const MSLLHOOKSTRUCT*>(l_param);
            const short delta = static_cast<short>(HIWORD(mouse_struct->mouseData));
            if (delta > 0) {
                api->emit_mouse_wheel_up();
            } else if (delta < 0) {
                api->emit_mouse_wheel_down();
            }
            break;
        }
        case WM_MOUSEHWHEEL: {
            const auto* mouse_struct = reinterpret_cast<const MSLLHOOKSTRUCT*>(l_param);
            const short delta = static_cast<short>(HIWORD(mouse_struct->mouseData));
            if (delta > 0) {
                api->emit_mouse_wheel_right();
            } else if (delta < 0) {
                api->emit_mouse_wheel_left();
            }
            break;
        }
        default:
            break;
    }

    return CallNextHookEx(nullptr, code, w_param, l_param);
}
} // namespace
#endif

MouseInputApi::~MouseInputApi() {
    stop_mouse_listener();
}

void MouseInputApi::set_event_handler(MouseEventHandler handler) {
    event_handler_ = std::move(handler);
}

void MouseInputApi::emit_mouse_left_button_down() {
    mouse_left_button_pressed_.store(true);
    publish(MouseEventType::mouse_left_button_down);
}

void MouseInputApi::emit_mouse_left_button_up() {
    mouse_left_button_pressed_.store(false);
    publish(MouseEventType::mouse_left_button_up);
}

void MouseInputApi::emit_mouse_right_button_down() {
    mouse_right_button_pressed_.store(true);
    publish(MouseEventType::mouse_right_button_down);
}

void MouseInputApi::emit_mouse_right_button_up() {
    mouse_right_button_pressed_.store(false);
    publish(MouseEventType::mouse_right_button_up);
}

void MouseInputApi::emit_mouse_middle_button_down() {
    mouse_middle_button_pressed_.store(true);
    publish(MouseEventType::mouse_middle_button_down);
}

void MouseInputApi::emit_mouse_middle_button_up() {
    mouse_middle_button_pressed_.store(false);
    publish(MouseEventType::mouse_middle_button_up);
}

void MouseInputApi::emit_mouse_side_button_1_down() {
    mouse_side_button_1_pressed_.store(true);
    publish(MouseEventType::mouse_side_button_1_down);
}

void MouseInputApi::emit_mouse_side_button_1_up() {
    mouse_side_button_1_pressed_.store(false);
    publish(MouseEventType::mouse_side_button_1_up);
}

void MouseInputApi::emit_mouse_side_button_2_down() {
    mouse_side_button_2_pressed_.store(true);
    publish(MouseEventType::mouse_side_button_2_down);
}

void MouseInputApi::emit_mouse_side_button_2_up() {
    mouse_side_button_2_pressed_.store(false);
    publish(MouseEventType::mouse_side_button_2_up);
}

void MouseInputApi::emit_mouse_wheel_up() {
    publish(MouseEventType::mouse_wheel_up);
}

void MouseInputApi::emit_mouse_wheel_down() {
    publish(MouseEventType::mouse_wheel_down);
}

void MouseInputApi::emit_mouse_wheel_left() {
    publish(MouseEventType::mouse_wheel_left);
}

void MouseInputApi::emit_mouse_wheel_right() {
    publish(MouseEventType::mouse_wheel_right);
}

bool MouseInputApi::start_mouse_listener() {
    if (hook_session_.is_running()) {
        return true;
    }

#if !defined(_WIN32)
    std::cout << "TODO: mouse listener currently implemented only for Windows" << std::endl;
    return false;
#else
    return hook_session_.start(
        WH_MOUSE_LL,
        windows_low_level_mouse_proc,
        [this]() {
            g_windows_mouse_api_instance = this;
        },
        []() {
            g_windows_mouse_api_instance = nullptr;
        });
#endif
}

bool MouseInputApi::start_mouse_left_button_listener() {
    return start_mouse_listener();
}

void MouseInputApi::stop_mouse_listener() {
    hook_session_.stop();
}

void MouseInputApi::stop_mouse_left_button_listener() {
    stop_mouse_listener();
}

bool MouseInputApi::is_mouse_left_button_pressed() const {
    return mouse_left_button_pressed_.load();
}

std::string MouseInputApi::describe_state() const {
    std::ostringstream stream;
    stream
        << "mouse_left_button=" << (mouse_left_button_pressed_.load() ? "pressed" : "released")
        << " mouse_right_button=" << (mouse_right_button_pressed_.load() ? "pressed" : "released")
        << " mouse_middle_button=" << (mouse_middle_button_pressed_.load() ? "pressed" : "released")
        << " mouse_side_button_1=" << (mouse_side_button_1_pressed_.load() ? "pressed" : "released")
        << " mouse_side_button_2=" << (mouse_side_button_2_pressed_.load() ? "pressed" : "released");
    return stream.str();
}

void MouseInputApi::publish(MouseEventType event_type) {
    if (!event_handler_) {
        return;
    }

#if defined(_WIN32)
    const ModifierState modifiers = current_modifier_state();
#else
    const ModifierState modifiers{false, false, false, false};
#endif

    event_handler_(MouseEvent{
        event_type,
        mouse_left_button_pressed_.load(),
        mouse_right_button_pressed_.load(),
        mouse_middle_button_pressed_.load(),
        mouse_side_button_1_pressed_.load(),
        mouse_side_button_2_pressed_.load(),
        modifiers.alt_pressed,
        modifiers.ctrl_pressed,
        modifiers.shift_pressed,
        modifiers.win_pressed});
}

} // namespace Keymera::functions::system::input_api
