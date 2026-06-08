#include "keyboard-input-api.hpp"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Keymera::functions::system::input_api {

#if defined(_WIN32)
namespace {
KeyboardInputApi* g_windows_keyboard_api_instance = nullptr;

LRESULT CALLBACK windows_low_level_keyboard_proc(int code, WPARAM w_param, LPARAM l_param) {
    if (code < 0 || g_windows_keyboard_api_instance == nullptr) {
        return CallNextHookEx(nullptr, code, w_param, l_param);
    }

    auto* api = g_windows_keyboard_api_instance;
    const auto* keyboard_struct = reinterpret_cast<const KBDLLHOOKSTRUCT*>(l_param);
    const bool is_extended_key = (keyboard_struct->flags & LLKHF_EXTENDED) != 0;

    switch (w_param) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            api->emit_key_down(keyboard_struct->vkCode, is_extended_key);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            api->emit_key_up(keyboard_struct->vkCode, is_extended_key);
            break;
        default:
            break;
    }

    return CallNextHookEx(nullptr, code, w_param, l_param);
}
} // namespace
#endif

KeyboardInputApi::~KeyboardInputApi() {
    stop_keyboard_listener();
}

void KeyboardInputApi::set_event_handler(KeyboardEventHandler handler) {
    event_handler_ = std::move(handler);
}

void KeyboardInputApi::emit_key_down(unsigned int virtual_key_code, bool is_extended_key) {
    publish(KeyboardEventType::key_down, virtual_key_code, is_extended_key);
}

void KeyboardInputApi::emit_key_up(unsigned int virtual_key_code, bool is_extended_key) {
    publish(KeyboardEventType::key_up, virtual_key_code, is_extended_key);
}

bool KeyboardInputApi::start_keyboard_listener() {
    if (hook_session_.is_running()) {
        return true;
    }

#if !defined(_WIN32)
    return false;
#else
    return hook_session_.start(
        WH_KEYBOARD_LL,
        windows_low_level_keyboard_proc,
        [this]() {
            g_windows_keyboard_api_instance = this;
        },
        []() {
            g_windows_keyboard_api_instance = nullptr;
        });
#endif
}

void KeyboardInputApi::stop_keyboard_listener() {
    hook_session_.stop();
}

void KeyboardInputApi::publish(KeyboardEventType event_type, unsigned int virtual_key_code, bool is_extended_key) {
    if (!event_handler_) {
        return;
    }

#if defined(_WIN32)
    const bool alt_pressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool ctrl_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool win_pressed = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
#else
    const bool alt_pressed = false;
    const bool ctrl_pressed = false;
    const bool shift_pressed = false;
    const bool win_pressed = false;
#endif

    event_handler_(KeyboardEvent{
        event_type,
        virtual_key_code,
        is_extended_key,
        alt_pressed,
        ctrl_pressed,
        shift_pressed,
        win_pressed});
}

} // namespace Keymera::functions::system::input_api
