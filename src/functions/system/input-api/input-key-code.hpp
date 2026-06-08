#pragma once

#include <cstdint>
#include <string>

namespace Keymera::functions::system::input_api {

/// Compact logical key identity.  All values fit in uint16_t.
/// Values are stable integers �?never re-use or reorder existing entries.
enum class KeyCode : uint16_t {
    // ── Letters (0-25) ──────────────────────────────────────────────────────
    A = 0, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // ── Digits (26-35) ──────────────────────────────────────────────────────
    k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,

    // ── Function keys (36-59) ───────────────────────────────────────────────
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
    F11, F12, F13, F14, F15, F16, F17, F18, F19, F20,
    F21, F22, F23, F24,

    // ── Navigation / editing (60-85) ────────────────────────────────────────
    ESC, ENTER, SPACE, TAB, BACKSPACE, CAPS,
    KEY_DELETE, INSERT, HOME, END, PAGE_UP, PAGE_DOWN,
    PRINT_SCREEN, SCROLL_LOCK, PAUSE, NUM_LOCK,
    SHIFT, CTRL, ALT, WIN,
    UP, DOWN, LEFT, RIGHT,
    APPS, SLEEP,

    // ── Punctuation (86-97) ─────────────────────────────────────────────────
    SEMICOLON, EQUAL, COMMA, MINUS, PERIOD, SLASH,
    BACKQUOTE, LBRACKET, RBRACKET, BACKSLASH, OEM_102, QUOTE,

    // ── Numpad (98-112) ─────────────────────────────────────────────────────
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4,
    NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
    NUM_MULTIPLY, NUM_ADD, NUM_SUBTRACT, NUM_DECIMAL, NUM_DIVIDE,

    // ── Media / Browser / Launch (113-130) ──────────────────────────────────
    VOLUME_MUTE, VOLUME_DOWN, VOLUME_UP,
    MEDIA_NEXT, MEDIA_PREV, MEDIA_STOP, MEDIA_PLAY_PAUSE,
    BROWSER_BACK, BROWSER_FORWARD, BROWSER_REFRESH, BROWSER_STOP,
    BROWSER_SEARCH, BROWSER_FAVORITES, BROWSER_HOME,
    LAUNCH_MAIL, LAUNCH_MEDIA, LAUNCH_APP_1, LAUNCH_APP_2,

    // ── Mouse buttons / wheel (131-139) ─────────────────────────────────────
    MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE,
    MOUSE_SIDE_1, MOUSE_SIDE_2,
    MOUSE_WHEEL_UP, MOUSE_WHEEL_DOWN,
    MOUSE_WHEEL_LEFT, MOUSE_WHEEL_RIGHT,

    // ── Sentinel ─────────────────────────────────────────────────────────────
    UNKNOWN,
    COUNT
};

/// Compact input action identity.
enum class InputAction : uint8_t {
    down = 0,
    up,
    scroll_up,
    scroll_down,
    scroll_left,
    scroll_right,
    unknown,
    COUNT
};

/// Convert an UPPERCASE key-name string to KeyCode.
/// Returns KeyCode::UNKNOWN for unrecognised names.
KeyCode key_code_from_string(const std::string& key_name_upper);

/// Convert a lowercase action string to InputAction.
/// Returns InputAction::unknown for unrecognised names.
InputAction input_action_from_string(const std::string& action_lower);

} // namespace Keymera::functions::system::input_api
