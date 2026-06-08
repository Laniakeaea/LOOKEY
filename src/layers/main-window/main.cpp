#include "../../functions/system/input-api/input-api.hpp"
#include "../../functions/system/input-api/input-event-codec.hpp"
#include "../../functions/system/input-middleware/input-semantics-middleware.hpp"
#include "../input-overlay-window/input-overlay-window.hpp"
#include "../input-overlay-window/theme/theme-config.hpp"
#include "./tray-settings.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace input_api = lookey::functions::system::input_api;
namespace input_middleware = lookey::functions::system::input_middleware;
namespace input_overlay_window = lookey::layers::input_overlay_window;
namespace input_overlay_theme_config = lookey::layers::input_overlay_window::theme_config;
namespace tray_settings = lookey::layers::main_window::tray_settings;

namespace {
#if defined(_WIN32)
constexpr UINT k_tray_callback_message = WM_APP + 42;
constexpr UINT k_tray_icon_uid = 1001;
constexpr UINT k_tray_menu_anchor_top_left = 40101;
constexpr UINT k_tray_menu_anchor_top_right = 40102;
constexpr UINT k_tray_menu_anchor_bottom_left = 40103;
constexpr UINT k_tray_menu_anchor_bottom_right = 40104;
constexpr UINT k_tray_menu_history_down = 40201;
constexpr UINT k_tray_menu_history_up = 40202;
constexpr UINT k_tray_menu_theme_dark = 40301;
constexpr UINT k_tray_menu_theme_light = 40302;
constexpr UINT k_tray_menu_scale_50 = 40450;
constexpr UINT k_tray_menu_scale_75 = 40475;
constexpr UINT k_tray_menu_scale_100 = 40500;
constexpr UINT k_tray_menu_scale_125 = 40525;
constexpr UINT k_tray_menu_scale_150 = 40550;
constexpr UINT k_tray_menu_scale_175 = 40575;
constexpr UINT k_tray_menu_scale_200 = 40600;
constexpr UINT k_tray_menu_startup_toggle = 40700;
constexpr UINT k_tray_menu_mouse_side_buttons_toggle = 40710;
constexpr UINT k_tray_menu_drag_mode_toggle = 40720;
constexpr UINT k_tray_menu_history_ignore_mouse_buttons_toggle = 40730;
constexpr UINT k_tray_menu_adjust_current_alpha_toggle = 40740;
constexpr UINT k_tray_menu_adjust_history_alpha_toggle = 40750;
constexpr UINT k_tray_menu_restore_factory = 40800;
constexpr UINT k_tray_menu_exit = 40999;

std::atomic<int> g_tray_anchor_request{-1};
std::atomic<int> g_tray_history_request{-1};
std::atomic<int> g_tray_theme_request{-1};
std::atomic<int> g_tray_scale_request{-1};
std::atomic<bool> g_tray_startup_toggle_requested{false};
std::atomic<bool> g_tray_startup_enabled{false};
std::atomic<bool> g_tray_mouse_side_buttons_toggle_requested{false};
std::atomic<bool> g_tray_mouse_side_buttons_enabled{true};
std::atomic<bool> g_tray_drag_mode_toggle_requested{false};
std::atomic<bool> g_tray_drag_mode_enabled{false};
std::atomic<bool> g_tray_history_ignore_mouse_buttons_toggle_requested{false};
std::atomic<bool> g_tray_history_ignore_mouse_buttons_enabled{false};
std::atomic<bool> g_tray_adjust_current_alpha_toggle_requested{false};
std::atomic<bool> g_tray_adjust_current_alpha_enabled{false};
std::atomic<bool> g_tray_adjust_history_alpha_toggle_requested{false};
std::atomic<bool> g_tray_adjust_history_alpha_enabled{false};
std::atomic<bool> g_tray_restore_factory_requested{false};
std::atomic<bool> g_tray_exit_requested{false};

constexpr const wchar_t* k_windows_startup_run_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* k_windows_startup_value_name = L"LOOKEY";

bool set_start_with_windows_enabled(bool enabled) {
    HKEY run_key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        k_windows_startup_run_key,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &run_key,
        nullptr);
    if (open_result != ERROR_SUCCESS || run_key == nullptr) {
        return false;
    }

    bool ok = true;
    if (enabled) {
        wchar_t exe_path[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            ok = false;
        } else {
            std::wstring command = L"\"";
            command += exe_path;
            command += L"\"";
            const LONG set_result = RegSetValueExW(
                run_key,
                k_windows_startup_value_name,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            ok = (set_result == ERROR_SUCCESS);
        }
    } else {
        const LONG delete_result = RegDeleteValueW(run_key, k_windows_startup_value_name);
        ok = (delete_result == ERROR_SUCCESS || delete_result == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(run_key);
    return ok;
}

bool query_start_with_windows_enabled(bool& enabled) {
    enabled = false;
    HKEY run_key = nullptr;
    const LONG open_result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        k_windows_startup_run_key,
        0,
        KEY_QUERY_VALUE,
        &run_key);
    if (open_result != ERROR_SUCCESS || run_key == nullptr) {
        return false;
    }

    LONG query_result = RegQueryValueExW(run_key, k_windows_startup_value_name, nullptr, nullptr, nullptr, nullptr);
    enabled = (query_result == ERROR_SUCCESS);
    RegCloseKey(run_key);
    return true;
}

LRESULT CALLBACK tray_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    if (msg == k_tray_callback_message) {
        if (l_param == WM_LBUTTONUP || l_param == WM_RBUTTONUP || l_param == WM_CONTEXTMENU) {
            HMENU menu = CreatePopupMenu();
            if (menu != nullptr) {
                HMENU anchor_menu = CreatePopupMenu();
                if (anchor_menu != nullptr) {
                    AppendMenuW(anchor_menu, MF_STRING, k_tray_menu_anchor_top_left, L"Anchor: Top Left");
                    AppendMenuW(anchor_menu, MF_STRING, k_tray_menu_anchor_top_right, L"Anchor: Top Right");
                    AppendMenuW(anchor_menu, MF_STRING, k_tray_menu_anchor_bottom_left, L"Anchor: Bottom Left");
                    AppendMenuW(anchor_menu, MF_STRING, k_tray_menu_anchor_bottom_right, L"Anchor: Bottom Right");
                    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(anchor_menu), L"Anchor");
                }

                HMENU history_menu = CreatePopupMenu();
                if (history_menu != nullptr) {
                    AppendMenuW(history_menu, MF_STRING, k_tray_menu_history_down, L"History: Down");
                    AppendMenuW(history_menu, MF_STRING, k_tray_menu_history_up, L"History: Up");
                    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(history_menu), L"History");
                }

                HMENU theme_menu = CreatePopupMenu();
                if (theme_menu != nullptr) {
                    AppendMenuW(theme_menu, MF_STRING, k_tray_menu_theme_dark, L"Theme: Dark");
                    AppendMenuW(theme_menu, MF_STRING, k_tray_menu_theme_light, L"Theme: Light");
                    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(theme_menu), L"Theme");
                }

                HMENU scale_menu = CreatePopupMenu();
                if (scale_menu != nullptr) {
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_50, L"50%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_75, L"75%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_100, L"100%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_125, L"125%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_150, L"150%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_175, L"175%");
                    AppendMenuW(scale_menu, MF_STRING, k_tray_menu_scale_200, L"200%");
                    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(scale_menu), L"Scale");
                }

                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_startup_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_startup_toggle,
                    L"Auto Launch at Login");
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_mouse_side_buttons_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_mouse_side_buttons_toggle,
                    L"Mouse Has Side Buttons");
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_drag_mode_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_drag_mode_toggle,
                    L"Drag Mode");
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_history_ignore_mouse_buttons_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_history_ignore_mouse_buttons_toggle,
                    L"History Ignores Mouse Buttons");
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_adjust_current_alpha_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_adjust_current_alpha_toggle,
                    L"Adjust Current Alpha");
                AppendMenuW(
                    menu,
                    MF_STRING | (g_tray_adjust_history_alpha_enabled.load() ? MF_CHECKED : MF_UNCHECKED),
                    k_tray_menu_adjust_history_alpha_toggle,
                    L"Adjust History Alpha");
                AppendMenuW(menu, MF_STRING, k_tray_menu_restore_factory, L"Restore Factory Defaults");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, k_tray_menu_exit, L"Exit");

                POINT cursor{};
                GetCursorPos(&cursor);
                SetForegroundWindow(hwnd);

                const UINT command = TrackPopupMenu(
                    menu,
                    TPM_RETURNCMD | TPM_NONOTIFY,
                    cursor.x,
                    cursor.y,
                    0,
                    hwnd,
                    nullptr);

                DestroyMenu(menu);

                if (command == k_tray_menu_anchor_top_left
                    || command == k_tray_menu_anchor_top_right
                    || command == k_tray_menu_anchor_bottom_left
                    || command == k_tray_menu_anchor_bottom_right) {
                    g_tray_anchor_request = static_cast<int>(command);
                } else if (command == k_tray_menu_history_down
                    || command == k_tray_menu_history_up) {
                    g_tray_history_request = static_cast<int>(command);
                } else if (command == k_tray_menu_theme_dark
                    || command == k_tray_menu_theme_light) {
                    g_tray_theme_request = static_cast<int>(command);
                } else if (command == k_tray_menu_scale_50
                    || command == k_tray_menu_scale_75
                    || command == k_tray_menu_scale_100
                    || command == k_tray_menu_scale_125
                    || command == k_tray_menu_scale_150
                    || command == k_tray_menu_scale_175
                    || command == k_tray_menu_scale_200) {
                    g_tray_scale_request = static_cast<int>(command);
                } else if (command == k_tray_menu_startup_toggle) {
                    g_tray_startup_toggle_requested = true;
                } else if (command == k_tray_menu_mouse_side_buttons_toggle) {
                    g_tray_mouse_side_buttons_toggle_requested = true;
                } else if (command == k_tray_menu_drag_mode_toggle) {
                    g_tray_drag_mode_enabled = !g_tray_drag_mode_enabled.load();
                    g_tray_drag_mode_toggle_requested = true;
                } else if (command == k_tray_menu_history_ignore_mouse_buttons_toggle) {
                    g_tray_history_ignore_mouse_buttons_toggle_requested = true;
                } else if (command == k_tray_menu_adjust_current_alpha_toggle) {
                    g_tray_adjust_current_alpha_toggle_requested = true;
                } else if (command == k_tray_menu_adjust_history_alpha_toggle) {
                    g_tray_adjust_history_alpha_toggle_requested = true;
                } else if (command == k_tray_menu_restore_factory) {
                    g_tray_restore_factory_requested = true;
                } else if (command == k_tray_menu_exit) {
                    g_tray_exit_requested = true;
                }
            }
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

class TrayIconController {
public:
    ~TrayIconController() {
        shutdown();
    }

    bool initialize() {
        if (initialized_) {
            return true;
        }

        HINSTANCE instance = GetModuleHandleW(nullptr);

        WNDCLASSW wc{};
        wc.lpfnWndProc = tray_window_proc;
        wc.hInstance = instance;
        wc.lpszClassName = L"LookeyTrayWindowClass";
        if (!RegisterClassW(&wc)) {
            const DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                return false;
            }
        }

        hwnd_ = CreateWindowExW(
            0,
            wc.lpszClassName,
            L"LookeyTrayWindow",
            WS_OVERLAPPED,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (hwnd_ == nullptr) {
            return false;
        }

        notify_icon_data_ = {};
        notify_icon_data_.cbSize = sizeof(notify_icon_data_);
        notify_icon_data_.hWnd = hwnd_;
        notify_icon_data_.uID = k_tray_icon_uid;
        notify_icon_data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        notify_icon_data_.uCallbackMessage = k_tray_callback_message;
        const int small_icon_width = GetSystemMetrics(SM_CXSMICON);
        const int small_icon_height = GetSystemMetrics(SM_CYSMICON);
        notify_icon_data_.hIcon = static_cast<HICON>(LoadImageW(
            instance,
            MAKEINTRESOURCEW(100),
            IMAGE_ICON,
            small_icon_width,
            small_icon_height,
            LR_DEFAULTCOLOR));
        if (notify_icon_data_.hIcon == nullptr) {
            notify_icon_data_.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        }
        lstrcpynW(notify_icon_data_.szTip, L"Lookey", sizeof(notify_icon_data_.szTip) / sizeof(WCHAR));

        if (!Shell_NotifyIconW(NIM_ADD, &notify_icon_data_)) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
            return false;
        }

        initialized_ = true;
        return true;
    }

    void pump_messages() {
        if (hwnd_ == nullptr) {
            return;
        }

        MSG msg{};
        while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    int consume_anchor_request() {
        return g_tray_anchor_request.exchange(-1);
    }

    int consume_history_request() {
        return g_tray_history_request.exchange(-1);
    }

    int consume_theme_request() {
        return g_tray_theme_request.exchange(-1);
    }

    int consume_scale_request() {
        return g_tray_scale_request.exchange(-1);
    }

    bool consume_startup_toggle_request() {
        return g_tray_startup_toggle_requested.exchange(false);
    }

    void set_startup_enabled_state(bool enabled) {
        g_tray_startup_enabled = enabled;
    }

    bool consume_mouse_side_buttons_toggle_request() {
        return g_tray_mouse_side_buttons_toggle_requested.exchange(false);
    }

    void set_mouse_side_buttons_enabled_state(bool enabled) {
        g_tray_mouse_side_buttons_enabled = enabled;
    }

    bool consume_drag_mode_toggle_request() {
        return g_tray_drag_mode_toggle_requested.exchange(false);
    }

    void set_drag_mode_enabled_state(bool enabled) {
        g_tray_drag_mode_enabled = enabled;
    }

    bool consume_history_ignore_mouse_buttons_toggle_request() {
        return g_tray_history_ignore_mouse_buttons_toggle_requested.exchange(false);
    }

    void set_history_ignore_mouse_buttons_enabled_state(bool enabled) {
        g_tray_history_ignore_mouse_buttons_enabled = enabled;
    }

    bool consume_adjust_current_alpha_toggle_request() {
        return g_tray_adjust_current_alpha_toggle_requested.exchange(false);
    }

    void set_adjust_current_alpha_enabled_state(bool enabled) {
        g_tray_adjust_current_alpha_enabled = enabled;
    }

    bool consume_adjust_history_alpha_toggle_request() {
        return g_tray_adjust_history_alpha_toggle_requested.exchange(false);
    }

    void set_adjust_history_alpha_enabled_state(bool enabled) {
        g_tray_adjust_history_alpha_enabled = enabled;
    }

    bool consume_restore_factory_request() {
        return g_tray_restore_factory_requested.exchange(false);
    }

    bool consume_exit_request() {
        return g_tray_exit_requested.exchange(false);
    }

    void shutdown() {
        if (!initialized_) {
            return;
        }

        Shell_NotifyIconW(NIM_DELETE, &notify_icon_data_);
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        initialized_ = false;
    }

private:
    HWND hwnd_{nullptr};
    NOTIFYICONDATAW notify_icon_data_{};
    bool initialized_{false};
};
#endif

constexpr bool k_enable_input_event_log = false;
constexpr auto k_sleep_with_settings_open = std::chrono::milliseconds(16);
constexpr auto k_sleep_with_recent_input = std::chrono::milliseconds(16);
constexpr auto k_sleep_when_idle = std::chrono::milliseconds(80);
constexpr auto k_recent_input_window = std::chrono::milliseconds(300);

char key_side_char(input_api::KeySide side) {
    switch (side) {
        case input_api::KeySide::left:
            return 'L';
        case input_api::KeySide::right:
            return 'R';
        default:
            return 'N';
    }
}

std::string repeat_kind_name(input_middleware::RepeatKind kind) {
    switch (kind) {
        case input_middleware::RepeatKind::hold:
            return "HOLD";
        case input_middleware::RepeatKind::tap:
            return "TAP";
        default:
            return "";
    }
}

void apply_overlay_position_from_settings(
    input_overlay_window::InputOverlayWindow& overlay_window,
    const tray_settings::GeneralSettings& general_settings) {
#if !defined(_WIN32)
    (void)overlay_window;
    (void)general_settings;
#else
    const bool right_aligned = general_settings.display_anchor == tray_settings::DisplayAnchor::top_right
        || general_settings.display_anchor == tray_settings::DisplayAnchor::bottom_right;

    if (general_settings.custom_position_enabled) {
        overlay_window.set_right_aligned_layout(right_aligned);
        overlay_window.set_position(general_settings.custom_position_x, general_settings.custom_position_y);
        return;
    }

    const auto style = overlay_window.style();
    RECT work_rect{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_rect, 0) == 0) {
        work_rect.left = 0;
        work_rect.top = 0;
        work_rect.right = GetSystemMetrics(SM_CXSCREEN);
        work_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int screen_w = work_rect.right - work_rect.left;
    const int screen_h = work_rect.bottom - work_rect.top;
    const int distance = general_settings.display_distance_px;

    int x = work_rect.left + distance;
    int y = work_rect.top + distance;

    if (general_settings.display_anchor == tray_settings::DisplayAnchor::top_right
        || general_settings.display_anchor == tray_settings::DisplayAnchor::bottom_right) {
        x = work_rect.left + screen_w - distance;
    }

    if (general_settings.display_anchor == tray_settings::DisplayAnchor::bottom_left
        || general_settings.display_anchor == tray_settings::DisplayAnchor::bottom_right) {
        y = work_rect.top + screen_h - distance - style.height;
    }

    overlay_window.set_right_aligned_layout(right_aligned);

    overlay_window.set_position(x, y);
#endif
}

void apply_visual_theme_from_settings(
    input_overlay_window::InputOverlayWindow& overlay_window,
    const tray_settings::GeneralSettings& general_settings) {
    auto overlay_style = overlay_window.style();
    auto key_style = overlay_window.normal_key_style();

    input_overlay_theme_config::apply_palette_to_styles(
        general_settings.theme == tray_settings::ThemeSetting::dark
            ? input_overlay_window::ThemeMode::dark
            : input_overlay_window::ThemeMode::light,
        overlay_style,
        key_style,
        overlay_style.color);

    overlay_window.set_style(overlay_style);
    overlay_window.set_normal_key_style(key_style);
    overlay_window.set_theme_mode(
        general_settings.theme == tray_settings::ThemeSetting::dark
            ? input_overlay_window::ThemeMode::dark
            : input_overlay_window::ThemeMode::light);
}

void apply_overlay_style_from_settings(
    input_overlay_window::InputOverlayWindow& overlay_window,
    const tray_settings::GeneralSettings& general_settings) {
    auto overlay_style = overlay_window.style();
    overlay_style.width = general_settings.overlay.width;
    overlay_style.height = general_settings.overlay.height;
    overlay_style.padding = general_settings.overlay.padding;
    overlay_style.spacing = general_settings.overlay.spacing;
    overlay_style.corner_radius = general_settings.overlay.corner_radius;
    overlay_style.opacity = static_cast<std::uint8_t>(general_settings.overlay.opacity);
    overlay_window.set_style(overlay_style);

    auto key_style = overlay_window.normal_key_style();
    key_style.size = general_settings.input.key_size;
    key_style.corner_radius = general_settings.input.key_corner_radius;
    key_style.opacity = static_cast<std::uint8_t>(general_settings.overlay.opacity);
    key_style.letter_font_size = general_settings.input.letter_font_size;
    key_style.function_font_size = general_settings.input.function_font_size;
    key_style.text_horizontal_padding = general_settings.input.text_horizontal_padding;
    key_style.font_opacity = static_cast<std::uint8_t>(general_settings.overlay.opacity);
    key_style.font_extra_bold_strength = general_settings.input.font_extra_bold_strength;
    key_style.mouse_status_panel_width = general_settings.input.mouse_status_panel_width;
    key_style.mouse_status_panel_height = general_settings.input.mouse_status_panel_height;
    key_style.mouse_status_panel_gap = general_settings.input.mouse_status_panel_gap;
    key_style.mouse_status_panel_opacity = static_cast<std::uint8_t>(general_settings.overlay.opacity);
    key_style.mouse_status_panel_font_size = general_settings.input.mouse_status_panel_font_size;
    key_style.mouse_has_side_buttons = general_settings.input.mouse_has_side_buttons;
    overlay_window.set_normal_key_style(key_style);

    overlay_window.set_idle_restore_delay_ms(static_cast<unsigned long long>(general_settings.overlay.idle_restore_delay_ms));
    overlay_window.set_history_display_limit(static_cast<unsigned int>(general_settings.overlay.history_limit));
    overlay_window.set_history_ignores_mouse_buttons(general_settings.input.history_ignores_mouse_buttons);
    overlay_window.set_history_item_scale(general_settings.overlay.history_item_scale);
    overlay_window.set_history_item_opacity(static_cast<std::uint8_t>(general_settings.overlay.history_item_opacity));
}

input_overlay_window::AlphaAdjustmentTarget alpha_adjustment_target_from_settings(
    const tray_settings::GeneralSettings& settings) {
    if (settings.adjust_current_alpha_enabled) {
        return input_overlay_window::AlphaAdjustmentTarget::current_display;
    }
    if (settings.adjust_history_alpha_enabled) {
        return input_overlay_window::AlphaAdjustmentTarget::history;
    }
    return input_overlay_window::AlphaAdjustmentTarget::none;
}

input_overlay_window::OverlayPosition current_anchor_position_for_settings(
    const input_overlay_window::InputOverlayWindow& overlay_window,
    const tray_settings::GeneralSettings& settings) {
    const auto position = overlay_window.window_screen_position();
    const auto size = overlay_window.window_screen_size();
    const bool right_aligned = settings.display_anchor == tray_settings::DisplayAnchor::top_right
        || settings.display_anchor == tray_settings::DisplayAnchor::bottom_right;

    return input_overlay_window::OverlayPosition{
        right_aligned ? (position.x + size.width) : position.x,
        position.y};
}

void apply_scale_to_general_settings(
    tray_settings::GeneralSettings& settings,
    int target_scale_percent,
    int current_scale_percent) {
    const int safe_current_scale_percent = current_scale_percent <= 0 ? 100 : current_scale_percent;
    const float factor = static_cast<float>(target_scale_percent) / static_cast<float>(safe_current_scale_percent);
    auto scale_int = [factor](int value) {
        return static_cast<int>(std::lround(static_cast<float>(value) * factor));
    };

    settings.display_distance_px = std::clamp(scale_int(settings.display_distance_px), 0, 300);

    settings.overlay.width = std::clamp(scale_int(settings.overlay.width), 40, 400);
    settings.overlay.height = std::clamp(scale_int(settings.overlay.height), 40, 400);
    settings.overlay.padding = std::clamp(scale_int(settings.overlay.padding), 0, 80);
    settings.overlay.spacing = std::clamp(scale_int(settings.overlay.spacing), 0, 80);
    settings.overlay.corner_radius = std::clamp(scale_int(settings.overlay.corner_radius), 0, 80);

    settings.input.key_size = std::clamp(scale_int(settings.input.key_size), 24, 200);
    settings.input.key_corner_radius = std::clamp(scale_int(settings.input.key_corner_radius), 0, 40);
    settings.input.letter_font_size = std::clamp(scale_int(settings.input.letter_font_size), 8, 96);
    settings.input.function_font_size = std::clamp(scale_int(settings.input.function_font_size), 8, 64);
    settings.input.text_horizontal_padding = std::clamp(scale_int(settings.input.text_horizontal_padding), 0, 64);
    settings.input.mouse_status_panel_width = std::clamp(scale_int(settings.input.mouse_status_panel_width), 8, 80);
    settings.input.mouse_status_panel_height = std::clamp(scale_int(settings.input.mouse_status_panel_height), 20, 160);
    settings.input.mouse_status_panel_gap = std::clamp(scale_int(settings.input.mouse_status_panel_gap), 0, 20);
    settings.input.mouse_status_panel_font_size = std::clamp(scale_int(settings.input.mouse_status_panel_font_size), 8, 48);
    settings.scale_percent = std::clamp(target_scale_percent, 50, 200);
}

std::filesystem::path tray_settings_path() {
    return std::filesystem::path("lookey-settings.cfg");
}

bool parse_bool_value(const std::string& text, bool& out_value) {
    if (text == "1" || text == "true" || text == "yes" || text == "on") {
        out_value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "no" || text == "off") {
        out_value = false;
        return true;
    }
    return false;
}

bool parse_int_value(const std::string& text, int& out_value) {
    try {
        out_value = std::stoi(text);
        return true;
    } catch (...) {
        return false;
    }
}

bool is_valid_assets_root(const std::filesystem::path& root) {
    return std::filesystem::exists(root / "assets" / "fonts" / "UbuntuMono" / "UbuntuMonoNerdFontMono-Bold.ttf")
        && std::filesystem::exists(root / "assets" / "fonts" / "UbuntuMono" / "UbuntuMonoNerdFontMono-BoldItalic.ttf")
        && std::filesystem::exists(root / "assets" / "icons" / "mouse_key_main_parts" / "mouse_key_left.svg");
}

std::filesystem::path executable_directory() {
#if defined(_WIN32)
    wchar_t module_path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(module_path).parent_path();
#else
    return {};
#endif
}

void ensure_assets_working_directory() {
#if defined(LOOKEY_EMBED_ASSETS)
    return;
#else
    std::error_code ec;
    const std::filesystem::path current = std::filesystem::current_path(ec);
    if (!ec && is_valid_assets_root(current)) {
        return;
    }

    const std::filesystem::path exe_dir = executable_directory();
    if (exe_dir.empty()) {
        return;
    }

    std::filesystem::path candidate = exe_dir;
    for (int level = 0; level < 6; ++level) {
        if (is_valid_assets_root(candidate)) {
            std::filesystem::current_path(candidate, ec);
            return;
        }

        if (!candidate.has_parent_path()) {
            break;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

#if defined(LOOKEY_SOURCE_ROOT)
    const std::filesystem::path source_root = std::filesystem::path(LOOKEY_SOURCE_ROOT);
    if (is_valid_assets_root(source_root)) {
        std::filesystem::current_path(source_root, ec);
    }
#endif
#endif
}

void save_tray_settings(const tray_settings::GeneralSettings& settings) {
    std::ofstream output(tray_settings_path(), std::ios::out | std::ios::trunc);
    if (!output) {
        return;
    }

    output << "theme=" << (settings.theme == tray_settings::ThemeSetting::dark ? "dark" : "light") << "\n";
    output << "start_with_windows=" << (settings.start_with_windows ? "1" : "0") << "\n";
    output << "scale_percent=" << settings.scale_percent << "\n";
    output << "drag_mode_enabled=" << (settings.drag_mode_enabled ? "1" : "0") << "\n";
    output << "adjust_current_alpha_enabled=" << (settings.adjust_current_alpha_enabled ? "1" : "0") << "\n";
    output << "adjust_history_alpha_enabled=" << (settings.adjust_history_alpha_enabled ? "1" : "0") << "\n";
    output << "custom_position_enabled=" << (settings.custom_position_enabled ? "1" : "0") << "\n";
    output << "custom_position_x=" << settings.custom_position_x << "\n";
    output << "custom_position_y=" << settings.custom_position_y << "\n";
    output << "display_anchor=";
    switch (settings.display_anchor) {
        case tray_settings::DisplayAnchor::top_left: output << "top_left"; break;
        case tray_settings::DisplayAnchor::top_right: output << "top_right"; break;
        case tray_settings::DisplayAnchor::bottom_left: output << "bottom_left"; break;
        case tray_settings::DisplayAnchor::bottom_right: output << "bottom_right"; break;
    }
    output << "\n";
    output << "history_direction=" << (settings.history_direction == tray_settings::HistoryDirection::up ? "up" : "down") << "\n";
    output << "display_distance_px=" << settings.display_distance_px << "\n";
    output << "input.mouse_has_side_buttons=" << (settings.input.mouse_has_side_buttons ? "1" : "0") << "\n";
    output << "input.history_ignores_mouse_buttons=" << (settings.input.history_ignores_mouse_buttons ? "1" : "0") << "\n";

    // Persist scaled dimensions so tray scale remains stable across restarts.
    output << "overlay.width=" << settings.overlay.width << "\n";
    output << "overlay.height=" << settings.overlay.height << "\n";
    output << "overlay.padding=" << settings.overlay.padding << "\n";
    output << "overlay.spacing=" << settings.overlay.spacing << "\n";
    output << "overlay.corner_radius=" << settings.overlay.corner_radius << "\n";
    output << "overlay.opacity=" << settings.overlay.opacity << "\n";
    output << "overlay.history_item_opacity=" << settings.overlay.history_item_opacity << "\n";
    output << "input.key_size=" << settings.input.key_size << "\n";
    output << "input.key_corner_radius=" << settings.input.key_corner_radius << "\n";
    output << "input.letter_font_size=" << settings.input.letter_font_size << "\n";
    output << "input.function_font_size=" << settings.input.function_font_size << "\n";
    output << "input.text_horizontal_padding=" << settings.input.text_horizontal_padding << "\n";
    output << "input.mouse_status_panel_width=" << settings.input.mouse_status_panel_width << "\n";
    output << "input.mouse_status_panel_height=" << settings.input.mouse_status_panel_height << "\n";
    output << "input.mouse_status_panel_gap=" << settings.input.mouse_status_panel_gap << "\n";
    output << "input.mouse_status_panel_font_size=" << settings.input.mouse_status_panel_font_size << "\n";
}

tray_settings::GeneralSettings load_tray_settings() {
    tray_settings::GeneralSettings settings{};
    std::ifstream input(tray_settings_path(), std::ios::in);
    if (!input) {
        return settings;
    }

    std::string line;
    while (std::getline(input, line)) {
        const std::size_t sep = line.find('=');
        if (sep == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, sep);
        const std::string value = line.substr(sep + 1);

        if (key == "theme") {
            settings.theme = (value == "light") ? tray_settings::ThemeSetting::light : tray_settings::ThemeSetting::dark;
        } else if (key == "start_with_windows") {
            parse_bool_value(value, settings.start_with_windows);
        } else if (key == "scale_percent") {
            parse_int_value(value, settings.scale_percent);
        } else if (key == "drag_mode_enabled") {
            parse_bool_value(value, settings.drag_mode_enabled);
        } else if (key == "adjust_current_alpha_enabled") {
            parse_bool_value(value, settings.adjust_current_alpha_enabled);
        } else if (key == "adjust_history_alpha_enabled") {
            parse_bool_value(value, settings.adjust_history_alpha_enabled);
        } else if (key == "custom_position_enabled") {
            parse_bool_value(value, settings.custom_position_enabled);
        } else if (key == "custom_position_x") {
            parse_int_value(value, settings.custom_position_x);
        } else if (key == "custom_position_y") {
            parse_int_value(value, settings.custom_position_y);
        } else if (key == "display_anchor") {
            if (value == "top_right") settings.display_anchor = tray_settings::DisplayAnchor::top_right;
            else if (value == "bottom_left") settings.display_anchor = tray_settings::DisplayAnchor::bottom_left;
            else if (value == "bottom_right") settings.display_anchor = tray_settings::DisplayAnchor::bottom_right;
            else settings.display_anchor = tray_settings::DisplayAnchor::top_left;
        } else if (key == "history_direction") {
            settings.history_direction = (value == "up") ? tray_settings::HistoryDirection::up : tray_settings::HistoryDirection::down;
        } else if (key == "display_distance_px") {
            parse_int_value(value, settings.display_distance_px);
        } else if (key == "input.mouse_has_side_buttons") {
            parse_bool_value(value, settings.input.mouse_has_side_buttons);
        } else if (key == "input.history_ignores_mouse_buttons") {
            parse_bool_value(value, settings.input.history_ignores_mouse_buttons);
        } else if (key == "overlay.width") {
            parse_int_value(value, settings.overlay.width);
        } else if (key == "overlay.height") {
            parse_int_value(value, settings.overlay.height);
        } else if (key == "overlay.padding") {
            parse_int_value(value, settings.overlay.padding);
        } else if (key == "overlay.spacing") {
            parse_int_value(value, settings.overlay.spacing);
        } else if (key == "overlay.corner_radius") {
            parse_int_value(value, settings.overlay.corner_radius);
        } else if (key == "overlay.opacity") {
            parse_int_value(value, settings.overlay.opacity);
        } else if (key == "overlay.history_item_opacity") {
            parse_int_value(value, settings.overlay.history_item_opacity);
        } else if (key == "input.key_size") {
            parse_int_value(value, settings.input.key_size);
        } else if (key == "input.key_corner_radius") {
            parse_int_value(value, settings.input.key_corner_radius);
        } else if (key == "input.letter_font_size") {
            parse_int_value(value, settings.input.letter_font_size);
        } else if (key == "input.function_font_size") {
            parse_int_value(value, settings.input.function_font_size);
        } else if (key == "input.text_horizontal_padding") {
            parse_int_value(value, settings.input.text_horizontal_padding);
        } else if (key == "input.mouse_status_panel_width") {
            parse_int_value(value, settings.input.mouse_status_panel_width);
        } else if (key == "input.mouse_status_panel_height") {
            parse_int_value(value, settings.input.mouse_status_panel_height);
        } else if (key == "input.mouse_status_panel_gap") {
            parse_int_value(value, settings.input.mouse_status_panel_gap);
        } else if (key == "input.mouse_status_panel_font_size") {
            parse_int_value(value, settings.input.mouse_status_panel_font_size);
        }
    }

    settings.scale_percent = std::clamp(settings.scale_percent, 50, 200);
    if (settings.adjust_current_alpha_enabled && settings.adjust_history_alpha_enabled) {
        settings.adjust_history_alpha_enabled = false;
    }
    settings.display_distance_px = std::clamp(settings.display_distance_px, 0, 300);
    settings.custom_position_x = std::clamp(settings.custom_position_x, -32768, 32768);
    settings.custom_position_y = std::clamp(settings.custom_position_y, -32768, 32768);

    // Guard against stale or malformed cfg values that can make the overlay invisible.
    settings.overlay.width = std::clamp(settings.overlay.width, 40, 400);
    settings.overlay.height = std::clamp(settings.overlay.height, 40, 400);
    settings.overlay.padding = std::clamp(settings.overlay.padding, 0, 80);
    settings.overlay.spacing = std::clamp(settings.overlay.spacing, 0, 80);
    settings.overlay.corner_radius = std::clamp(settings.overlay.corner_radius, 0, 80);
    settings.overlay.opacity = std::clamp(settings.overlay.opacity, 77, 255);
    settings.overlay.idle_restore_delay_ms = std::clamp(settings.overlay.idle_restore_delay_ms, 50, 5000);
    settings.overlay.history_limit = std::clamp(settings.overlay.history_limit, 1, 32);
    settings.overlay.history_item_scale = std::clamp(settings.overlay.history_item_scale, 0.3f, 1.0f);
    settings.overlay.history_item_opacity = std::clamp(settings.overlay.history_item_opacity, 77, 255);

    settings.input.key_size = std::clamp(settings.input.key_size, 24, 200);
    settings.input.key_corner_radius = std::clamp(settings.input.key_corner_radius, 0, 40);
    settings.input.letter_font_size = std::clamp(settings.input.letter_font_size, 8, 96);
    settings.input.function_font_size = std::clamp(settings.input.function_font_size, 8, 64);
    settings.input.text_horizontal_padding = std::clamp(settings.input.text_horizontal_padding, 0, 64);
    settings.input.font_opacity = std::clamp(settings.input.font_opacity, 16, 255);
    settings.input.font_extra_bold_strength = std::clamp(settings.input.font_extra_bold_strength, 0, 8);
    settings.input.mouse_status_panel_width = std::clamp(settings.input.mouse_status_panel_width, 8, 80);
    settings.input.mouse_status_panel_height = std::clamp(settings.input.mouse_status_panel_height, 20, 160);
    settings.input.mouse_status_panel_gap = std::clamp(settings.input.mouse_status_panel_gap, 0, 20);
    settings.input.mouse_status_panel_opacity = std::clamp(settings.input.mouse_status_panel_opacity, 16, 255);
    settings.input.mouse_status_panel_font_size = std::clamp(settings.input.mouse_status_panel_font_size, 8, 48);

    return settings;
}
}

int main() {
    ensure_assets_working_directory();

#if defined(_WIN32)
    HANDLE single_instance_mutex = CreateMutexW(nullptr, FALSE, L"Local\\LOOKEY_SINGLE_INSTANCE_MUTEX");
    if (single_instance_mutex == nullptr) {
        std::cout << "single-instance mutex failed" << std::endl;
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(single_instance_mutex);
        return 0;
    }
#endif

    std::atomic<bool> should_stop{false};
    std::atomic<unsigned long long> last_input_event_ms{0};

    const auto now_ms = []() -> unsigned long long {
        return static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    };

    input_api::InputApi input_api_core;
    input_middleware::InputSemanticsMiddleware semantics_middleware;
    input_overlay_window::InputOverlayWindow overlay_window;
#if defined(_WIN32)
    TrayIconController tray_controller;
#endif

    tray_settings::GeneralSettings general_settings_state = load_tray_settings();

    overlay_window.set_position(24, 24);
    overlay_window.set_style(input_overlay_window::OverlayStyle{
        .width = 80,
        .height = 80,
        .padding = 10,
        .spacing = 10,
        .corner_radius = 20,
        .opacity = 255,
        .color = input_overlay_window::OverlayColor{0, 0, 0},
    });
    overlay_window.set_normal_key_style(input_overlay_window::OverlayKeyStyle{
        .size = 60,
        .corner_radius = 10,
        .opacity = 255,
        .color = input_overlay_window::OverlayColor{255, 255, 255},
        .font_file_path = "assets/fonts/UbuntuMono/UbuntuMonoNerdFont-Regular.ttf",
        .font_bold_file_path = "assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-Bold.ttf",
        .font_bold_italic_file_path = "assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-BoldItalic.ttf",
        .letter_font_size = 36,
        .function_font_size = 20,
        .text_horizontal_padding = 12,
        .font_bold = true,
        .font_extra_bold_strength = 0,
        .font_opacity = 255,
        .font_color = input_overlay_window::OverlayColor{77, 77, 77},
        .text_supersample_scale = 6,
        .mouse_icon_supersample_scale = 6,
        .mouse_svg_curve_segments = 60,
        .mouse_status_panel_enabled = true,
        .mouse_status_panel_width = 20,
        .mouse_status_panel_height = 60,
        .mouse_status_panel_gap = 4,
        .mouse_status_panel_opacity = 255,
        .mouse_status_panel_bg_color = input_overlay_window::OverlayColor{0, 0, 0},
        .mouse_status_panel_font_size = 16,
        .mouse_status_panel_underline_thickness = 0,
        .mouse_status_panel_active_color = input_overlay_window::OverlayColor{232, 154, 60},
        .mouse_status_panel_inactive_color = input_overlay_window::OverlayColor{43, 29, 17},
        .function_text_bottom_margin = 5,
        .function_key_display_mode = input_overlay_window::FunctionKeyDisplayMode::key_name_only,
    });

    overlay_window.set_caps_lock_active(false);
    overlay_window.set_idle_restore_delay_ms(450);
    overlay_window.set_history_display_limit(6);

    apply_overlay_style_from_settings(overlay_window, general_settings_state);
    apply_visual_theme_from_settings(overlay_window, general_settings_state);
    apply_overlay_position_from_settings(overlay_window, general_settings_state);
    overlay_window.set_history_layout_direction(
        general_settings_state.history_direction == tray_settings::HistoryDirection::up
            ? input_overlay_window::HistoryLayoutDirection::up
            : input_overlay_window::HistoryLayoutDirection::down);

    if (!overlay_window.initialize()) {
        std::cout << "overlay window failed to start" << std::endl;
        return 1;
    }

#if defined(_WIN32)
    if (!tray_controller.initialize()) {
        std::cout << "tray icon failed to start" << std::endl;
    }
#endif

    semantics_middleware.set_repeat_window_ms(450);
    semantics_middleware.clear_exit_trigger();

    semantics_middleware.set_output_handler([&should_stop, &semantics_middleware, &overlay_window, &last_input_event_ms, &now_ms](const input_middleware::SemanticInputEvent& event) {
        last_input_event_ms = now_ms();

        if (k_enable_input_event_log) {
            const std::string encoded_line = input_api::encode_input_event(event.base_event) + " | combo=" + event.combo;
            std::cout << encoded_line << std::endl;
        }

        overlay_window.enqueue_input_event(
            event.base_event.key,
            event.base_event.action,
            event.combo,
            repeat_kind_name(event.repeat_kind),
            event.repeat_count,
            key_side_char(event.base_event.key_side),
            event.base_event.left_button_pressed,
            event.base_event.right_button_pressed,
            event.base_event.middle_button_pressed,
            event.base_event.side_button_1_pressed,
            event.base_event.side_button_2_pressed,
            event.base_event.ctrl_pressed,
            event.base_event.alt_pressed,
            event.base_event.shift_pressed,
            event.base_event.win_pressed);

        if (semantics_middleware.should_request_exit(event)) {
            should_stop = true;
        }
    });

    input_api_core.set_event_handler([&semantics_middleware](const input_api::UnifiedInputEvent& event) {
        semantics_middleware.process_event(event);
    });

    std::cout << "lookey api-first core test started" << std::endl;
    std::cout << "initial state: " << input_api_core.describe_state() << std::endl;

    if (!input_api_core.start_input_listener()) {
        std::cout << "input listener failed to start" << std::endl;
        return 1;
    }

    std::cout << "listener started: keyboard + mouse events are live" << std::endl;
    std::cout << "settings lives in system tray" << std::endl;

    unsigned long long applied_general_version = std::numeric_limits<unsigned long long>::max();
    std::optional<tray_settings::DisplayAnchor> tray_anchor_override;
    std::optional<tray_settings::HistoryDirection> tray_history_override;
    std::optional<tray_settings::ThemeSetting> tray_theme_override;
    int tray_scale_percent = general_settings_state.scale_percent;
    int tray_applied_scale_percent = tray_scale_percent;
    bool tray_force_factory_defaults = false;
    bool applied_start_with_windows = false;
    bool applied_mouse_side_buttons = general_settings_state.input.mouse_has_side_buttons;

#if defined(_WIN32)
    int tray_anchor_request = -1;
    int tray_history_request = -1;
    int tray_theme_request = -1;
    int tray_scale_request = -1;
    bool tray_startup_toggle_request = false;
    bool tray_mouse_side_buttons_toggle_request = false;
    bool tray_drag_mode_toggle_request = false;
    bool tray_history_ignore_mouse_buttons_toggle_request = false;
    bool tray_adjust_current_alpha_toggle_request = false;
    bool tray_adjust_history_alpha_toggle_request = false;
    bool tray_restore_factory_request = false;
    bool tray_exit_requested = false;
#endif

    {
        const auto& startup_settings = general_settings_state;
        const bool startup_ok = set_start_with_windows_enabled(startup_settings.start_with_windows);
        if (!startup_ok && k_enable_input_event_log) {
            std::cout << "startup registry update failed" << std::endl;
        }
        bool registry_state = startup_settings.start_with_windows;
        if (query_start_with_windows_enabled(registry_state)) {
            applied_start_with_windows = registry_state;
        } else {
            applied_start_with_windows = startup_settings.start_with_windows;
        }
        applied_mouse_side_buttons = startup_settings.input.mouse_has_side_buttons;
        tray_controller.set_startup_enabled_state(applied_start_with_windows);
        tray_controller.set_mouse_side_buttons_enabled_state(applied_mouse_side_buttons);
        tray_controller.set_drag_mode_enabled_state(startup_settings.drag_mode_enabled);
        tray_controller.set_history_ignore_mouse_buttons_enabled_state(startup_settings.input.history_ignores_mouse_buttons);
        tray_controller.set_adjust_current_alpha_enabled_state(startup_settings.adjust_current_alpha_enabled);
        tray_controller.set_adjust_history_alpha_enabled_state(startup_settings.adjust_history_alpha_enabled);
        overlay_window.set_drag_mode_enabled(startup_settings.drag_mode_enabled);
        overlay_window.set_alpha_adjustment_target(alpha_adjustment_target_from_settings(startup_settings));
    }

    while (overlay_window.pump_messages(should_stop)) {
        bool tray_changed_settings = false;
#if defined(_WIN32)
        tray_controller.pump_messages();
        tray_anchor_request = tray_controller.consume_anchor_request();
        tray_history_request = tray_controller.consume_history_request();
        tray_theme_request = tray_controller.consume_theme_request();
        tray_scale_request = tray_controller.consume_scale_request();
        tray_startup_toggle_request = tray_controller.consume_startup_toggle_request();
        tray_mouse_side_buttons_toggle_request = tray_controller.consume_mouse_side_buttons_toggle_request();
        tray_drag_mode_toggle_request = tray_controller.consume_drag_mode_toggle_request();
        tray_history_ignore_mouse_buttons_toggle_request = tray_controller.consume_history_ignore_mouse_buttons_toggle_request();
        tray_adjust_current_alpha_toggle_request = tray_controller.consume_adjust_current_alpha_toggle_request();
        tray_adjust_history_alpha_toggle_request = tray_controller.consume_adjust_history_alpha_toggle_request();
        tray_restore_factory_request = tray_controller.consume_restore_factory_request();
        tray_exit_requested = tray_controller.consume_exit_request();

        if (tray_anchor_request != -1) {
            if (tray_anchor_request == static_cast<int>(k_tray_menu_anchor_top_left)) {
                tray_anchor_override = tray_settings::DisplayAnchor::top_left;
            } else if (tray_anchor_request == static_cast<int>(k_tray_menu_anchor_top_right)) {
                tray_anchor_override = tray_settings::DisplayAnchor::top_right;
            } else if (tray_anchor_request == static_cast<int>(k_tray_menu_anchor_bottom_left)) {
                tray_anchor_override = tray_settings::DisplayAnchor::bottom_left;
            } else if (tray_anchor_request == static_cast<int>(k_tray_menu_anchor_bottom_right)) {
                tray_anchor_override = tray_settings::DisplayAnchor::bottom_right;
            }
            general_settings_state.custom_position_enabled = false;
            general_settings_state.drag_mode_enabled = false;
            overlay_window.set_drag_mode_enabled(false);
            tray_controller.set_drag_mode_enabled_state(false);
            tray_changed_settings = true;
        }

        if (tray_history_request != -1) {
            if (tray_history_request == static_cast<int>(k_tray_menu_history_down)) {
                tray_history_override = tray_settings::HistoryDirection::down;
            } else if (tray_history_request == static_cast<int>(k_tray_menu_history_up)) {
                tray_history_override = tray_settings::HistoryDirection::up;
            }
            tray_changed_settings = true;
        }

        if (tray_theme_request != -1) {
            if (tray_theme_request == static_cast<int>(k_tray_menu_theme_dark)) {
                tray_theme_override = tray_settings::ThemeSetting::dark;
            } else if (tray_theme_request == static_cast<int>(k_tray_menu_theme_light)) {
                tray_theme_override = tray_settings::ThemeSetting::light;
            }
            tray_changed_settings = true;
        }

        if (tray_scale_request != -1) {
            const int previous_tray_scale_percent = tray_scale_percent;
            if (tray_scale_request == static_cast<int>(k_tray_menu_scale_50)) tray_scale_percent = 50;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_75)) tray_scale_percent = 75;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_100)) tray_scale_percent = 100;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_125)) tray_scale_percent = 125;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_150)) tray_scale_percent = 150;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_175)) tray_scale_percent = 175;
            else if (tray_scale_request == static_cast<int>(k_tray_menu_scale_200)) tray_scale_percent = 200;
            if (tray_scale_percent != previous_tray_scale_percent) {
                tray_changed_settings = true;
            }
        }

        if (tray_startup_toggle_request) {
            tray_changed_settings = true;
        }

        if (tray_mouse_side_buttons_toggle_request) {
            tray_changed_settings = true;
        }

        if (tray_drag_mode_toggle_request) {
            tray_changed_settings = true;
        }

        if (tray_history_ignore_mouse_buttons_toggle_request) {
            tray_changed_settings = true;
        }

        if (tray_adjust_current_alpha_toggle_request || tray_adjust_history_alpha_toggle_request) {
            tray_changed_settings = true;
        }

        if (tray_restore_factory_request) {
            tray_anchor_override.reset();
            tray_history_override.reset();
            tray_theme_override.reset();
            tray_scale_percent = 100;
            tray_applied_scale_percent = 100;
            tray_force_factory_defaults = true;
            tray_changed_settings = true;
        }

        if (tray_exit_requested) {
            should_stop = true;
        }
#endif

        tray_settings::GeneralSettings general_settings = general_settings_state;
        if (tray_force_factory_defaults) {
            general_settings = tray_settings::GeneralSettings{};
            tray_force_factory_defaults = false;
        }

        if (tray_startup_toggle_request) {
            general_settings.start_with_windows = !applied_start_with_windows;
        }
        if (tray_mouse_side_buttons_toggle_request) {
            general_settings.input.mouse_has_side_buttons = !applied_mouse_side_buttons;
        }
        if (tray_drag_mode_toggle_request) {
            general_settings.drag_mode_enabled = !general_settings.drag_mode_enabled;
            if (general_settings.drag_mode_enabled) {
                const auto position = current_anchor_position_for_settings(overlay_window, general_settings);
                general_settings.custom_position_enabled = true;
                general_settings.custom_position_x = position.x;
                general_settings.custom_position_y = position.y;
            }
        }
        if (tray_history_ignore_mouse_buttons_toggle_request) {
            general_settings.input.history_ignores_mouse_buttons = !general_settings.input.history_ignores_mouse_buttons;
        }
        if (tray_adjust_current_alpha_toggle_request) {
            general_settings.adjust_current_alpha_enabled = !general_settings.adjust_current_alpha_enabled;
            if (general_settings.adjust_current_alpha_enabled) {
                general_settings.adjust_history_alpha_enabled = false;
            }
        }
        if (tray_adjust_history_alpha_toggle_request) {
            general_settings.adjust_history_alpha_enabled = !general_settings.adjust_history_alpha_enabled;
            if (general_settings.adjust_history_alpha_enabled) {
                general_settings.adjust_current_alpha_enabled = false;
            }
        }
        if (tray_anchor_override.has_value()) {
            general_settings.display_anchor = tray_anchor_override.value();
            general_settings.custom_position_enabled = false;
            general_settings.drag_mode_enabled = false;
        }
        if (tray_history_override.has_value()) {
            general_settings.history_direction = tray_history_override.value();
        }
        if (tray_theme_override.has_value()) {
            general_settings.theme = tray_theme_override.value();
        }
        if (tray_scale_request != -1 && tray_scale_percent != tray_applied_scale_percent) {
            apply_scale_to_general_settings(general_settings, tray_scale_percent, tray_applied_scale_percent);
            tray_applied_scale_percent = tray_scale_percent;
        }

        if (general_settings.drag_mode_enabled && overlay_window.consume_drag_position_changed()) {
            const auto position = current_anchor_position_for_settings(overlay_window, general_settings);
            if (!general_settings.custom_position_enabled
                || general_settings.custom_position_x != position.x
                || general_settings.custom_position_y != position.y) {
                general_settings.custom_position_enabled = true;
                general_settings.custom_position_x = position.x;
                general_settings.custom_position_y = position.y;
                tray_changed_settings = true;
            }
        }

        const int alpha_adjustment_steps = overlay_window.consume_alpha_adjustment_steps();
        if (alpha_adjustment_steps != 0) {
            constexpr int k_alpha_step = 13;
            constexpr int k_min_alpha = 77;
            if (general_settings.adjust_current_alpha_enabled) {
                const int next_alpha = std::clamp(
                    general_settings.overlay.opacity + (alpha_adjustment_steps * k_alpha_step),
                    k_min_alpha,
                    255);
                if (general_settings.overlay.opacity != next_alpha) {
                    general_settings.overlay.opacity = next_alpha;
                    tray_changed_settings = true;
                }
            } else if (general_settings.adjust_history_alpha_enabled) {
                const int next_alpha = std::clamp(
                    general_settings.overlay.history_item_opacity + (alpha_adjustment_steps * k_alpha_step),
                    k_min_alpha,
                    255);
                if (general_settings.overlay.history_item_opacity != next_alpha) {
                    general_settings.overlay.history_item_opacity = next_alpha;
                    tray_changed_settings = true;
                }
            }
        }

        if (general_settings.version != applied_general_version || tray_changed_settings) {
            apply_overlay_style_from_settings(overlay_window, general_settings);
            apply_visual_theme_from_settings(overlay_window, general_settings);
            apply_overlay_position_from_settings(overlay_window, general_settings);
            overlay_window.set_history_layout_direction(
                general_settings.history_direction == tray_settings::HistoryDirection::up
                    ? input_overlay_window::HistoryLayoutDirection::up
                    : input_overlay_window::HistoryLayoutDirection::down);

            if (tray_changed_settings) {
                general_settings_state = general_settings;
                save_tray_settings(general_settings_state);
            }

            if (general_settings.start_with_windows != applied_start_with_windows) {
                if (set_start_with_windows_enabled(general_settings.start_with_windows)) {
                    applied_start_with_windows = general_settings.start_with_windows;
                } else if (k_enable_input_event_log) {
                    std::cout << "startup registry update failed" << std::endl;
                }
            }
            tray_controller.set_startup_enabled_state(applied_start_with_windows);

            applied_mouse_side_buttons = general_settings.input.mouse_has_side_buttons;
            tray_controller.set_mouse_side_buttons_enabled_state(applied_mouse_side_buttons);
            overlay_window.set_drag_mode_enabled(general_settings.drag_mode_enabled);
            tray_controller.set_drag_mode_enabled_state(general_settings.drag_mode_enabled);
            tray_controller.set_history_ignore_mouse_buttons_enabled_state(general_settings.input.history_ignores_mouse_buttons);
            overlay_window.set_alpha_adjustment_target(alpha_adjustment_target_from_settings(general_settings));
            tray_controller.set_adjust_current_alpha_enabled_state(general_settings.adjust_current_alpha_enabled);
            tray_controller.set_adjust_history_alpha_enabled_state(general_settings.adjust_history_alpha_enabled);

            tray_scale_percent = general_settings.scale_percent;
            tray_applied_scale_percent = general_settings.scale_percent;
            applied_general_version = general_settings.version;
        }

        const unsigned long long current_ms = now_ms();
        const unsigned long long recent_event_ms = last_input_event_ms.load();
        const bool has_recent_input = recent_event_ms > 0
            && current_ms >= recent_event_ms
            && (current_ms - recent_event_ms) <= static_cast<unsigned long long>(k_recent_input_window.count());

        std::this_thread::sleep_for(
            has_recent_input ? k_sleep_with_recent_input : k_sleep_when_idle);
    }

    input_api_core.stop_input_listener();
#if defined(_WIN32)
    tray_controller.shutdown();
#endif
    overlay_window.request_close();
    std::cout << "listener stopped" << std::endl;

#if defined(_WIN32)
    CloseHandle(single_instance_mutex);
#endif

    return 0;
}

