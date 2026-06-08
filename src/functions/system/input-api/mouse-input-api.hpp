#pragma once

#include "windows-hook-session.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace Keymera::functions::system::input_api {

enum class MouseEventType {
    mouse_left_button_down,
    mouse_left_button_up,
    mouse_right_button_down,
    mouse_right_button_up,
    mouse_middle_button_down,
    mouse_middle_button_up,
    mouse_side_button_1_down,
    mouse_side_button_1_up,
    mouse_side_button_2_down,
    mouse_side_button_2_up,
    mouse_wheel_up,
    mouse_wheel_down,
    mouse_wheel_left,
    mouse_wheel_right,
};

struct MouseEvent {
    MouseEventType type;
    bool left_button_pressed;
    bool right_button_pressed;
    bool middle_button_pressed;
    bool side_button_1_pressed;
    bool side_button_2_pressed;
    bool alt_pressed;
    bool ctrl_pressed;
    bool shift_pressed;
    bool win_pressed;
};

using MouseEventHandler = std::function<void(const MouseEvent&)>;

class MouseInputApi {
public:
    MouseInputApi() = default;
    ~MouseInputApi();

    void set_event_handler(MouseEventHandler handler);

    void emit_mouse_left_button_down();
    void emit_mouse_left_button_up();
    void emit_mouse_right_button_down();
    void emit_mouse_right_button_up();
    void emit_mouse_middle_button_down();
    void emit_mouse_middle_button_up();
    void emit_mouse_side_button_1_down();
    void emit_mouse_side_button_1_up();
    void emit_mouse_side_button_2_down();
    void emit_mouse_side_button_2_up();
    void emit_mouse_wheel_up();
    void emit_mouse_wheel_down();
    void emit_mouse_wheel_left();
    void emit_mouse_wheel_right();

    bool start_mouse_listener();
    bool start_mouse_left_button_listener();
    void stop_mouse_left_button_listener();
    void stop_mouse_listener();

    [[nodiscard]] bool is_mouse_left_button_pressed() const;
    [[nodiscard]] std::string describe_state() const;

private:
    void publish(MouseEventType event_type);

    std::atomic<bool> mouse_left_button_pressed_{false};
    std::atomic<bool> mouse_right_button_pressed_{false};
    std::atomic<bool> mouse_middle_button_pressed_{false};
    std::atomic<bool> mouse_side_button_1_pressed_{false};
    std::atomic<bool> mouse_side_button_2_pressed_{false};

    WindowsHookSession hook_session_;

    MouseEventHandler event_handler_;
};

} // namespace Keymera::functions::system::input_api
