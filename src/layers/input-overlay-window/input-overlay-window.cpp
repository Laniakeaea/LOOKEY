#include "input-overlay-window.hpp"
#include "input-overlay-layout.hpp"
#include "input-overlay-render.hpp"
#include "style/style-sanitizer.hpp"
#include "model/overlay-frame-metrics.hpp"

#include "../../common/time/time-source.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <gdiplus.h>
#include <imm.h>
#endif

#include <filesystem>
#include <fstream>
#include <memory>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Keymera::layers::input_overlay_window {

#if defined(_WIN32)
namespace {
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::PrivateFontCollection;
using Gdiplus::RectF;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringAlignmentFar;
using Gdiplus::StringFormat;
using Gdiplus::SolidBrush;
using Gdiplus::SmoothingModeAntiAlias;

using namespace Keymera::layers::input_overlay_window::layout;
using namespace Keymera::layers::input_overlay_window::render;
using Keymera::common::time::now_epoch_ms;
using Keymera::layers::input_overlay_window::style::sanitize_overlay_style;
using Keymera::layers::input_overlay_window::style::sanitize_overlay_key_style;
using Keymera::layers::input_overlay_window::model::build_overlay_frame_metrics;

constexpr const char* k_overlay_window_class_name = "KEYMERA_INPUT_OVERLAY_WINDOW";
constexpr const char* k_app_big_icon_path = "assets/app/AppLogo.png";
constexpr const char* k_app_small_icon_path = "assets/app/AppLog_mid.png";
constexpr const char* k_waiting_key_label = "󰟢";

// now_epoch_ms comes from common::time via the using declaration above

bool is_idle_placeholder_label(const std::string& key_label) {
    return key_label == k_waiting_key_label;
}

bool is_keyboard_modifier_key_name(const std::string& key_name_upper) {
    return key_name_upper == "CTRL"
        || key_name_upper == "ALT"
        || key_name_upper == "SHIFT"
        || key_name_upper == "WIN";
}

void append_token_if_present(
    std::vector<std::string>& tokens,
    const std::unordered_set<std::string>& keys,
    const std::string& key) {
    if (keys.find(key) != keys.end()) {
        tokens.push_back(key);
    }
}

void set_active_key_from_snapshot(
    std::unordered_set<std::string>& keys,
    const std::string& key,
    bool is_active) {
    if (is_active) {
        keys.insert(key);
    } else {
        keys.erase(key);
    }
}

float clamp_history_item_scale(float scale) {
    if (scale < 0.3f) return 0.3f;
    if (scale > 1.0f) return 1.0f;
    return scale;
}

struct CachedFontFamilies {
    std::string path;
    std::unique_ptr<PrivateFontCollection> collection;
    std::unique_ptr<FontFamily[]> families;
    int count{0};
};

const CachedFontFamilies& load_cached_font_families(
    const std::string& font_file_path,
    CachedFontFamilies& cache) {
    if (cache.path == font_file_path && cache.collection && cache.count > 0) {
        return cache;
    }

    cache.path = font_file_path;
    cache.collection = std::make_unique<PrivateFontCollection>();
    cache.families.reset();
    cache.count = 0;

    if (!load_font_collection_from_file(font_file_path, *cache.collection)) {
        return cache;
    }

    const int family_count = cache.collection->GetFamilyCount();
    if (family_count <= 0) {
        return cache;
    }

    cache.families = std::make_unique<FontFamily[]>(family_count);
    cache.collection->GetFamilies(family_count, cache.families.get(), &cache.count);
    return cache;
}

HICON load_app_icon_from_png(const char* icon_path, int fallback_resource_id, int target_cx, int target_cy) {
    static std::unordered_map<uint64_t, HICON> icon_cache;
    const uint64_t cache_key = (static_cast<uint64_t>(fallback_resource_id) << 32)
        | (static_cast<uint32_t>(target_cx) << 16)
        | static_cast<uint32_t>(target_cy);
    auto cached_it = icon_cache.find(cache_key);
    if (cached_it != icon_cache.end()) {
        return cached_it->second;
    }

    if (icon_path != nullptr && icon_path[0] != '\0') {
        const std::wstring wide_path = utf8_to_utf16(icon_path);
        if (!wide_path.empty()) {
            Gdiplus::Bitmap bitmap(wide_path.c_str());
            if (bitmap.GetLastStatus() == Gdiplus::Ok) {
                const int bitmap_w = bitmap.GetWidth();
                const int bitmap_h = bitmap.GetHeight();
                HICON icon_handle = nullptr;
                if (bitmap_w == target_cx && bitmap_h == target_cy) {
                    if (bitmap.GetHICON(&icon_handle) == Gdiplus::Ok) {
                        icon_cache[cache_key] = icon_handle;
                        return icon_handle;
                    }
                } else {
                    Gdiplus::Bitmap resized(target_cx, target_cy, PixelFormat32bppARGB);
                    Gdiplus::Graphics graphics(&resized);
                    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
                    graphics.DrawImage(&bitmap, 0, 0, target_cx, target_cy);
                    if (resized.GetHICON(&icon_handle) == Gdiplus::Ok) {
                        icon_cache[cache_key] = icon_handle;
                        return icon_handle;
                    }
                }
            }
        }
    }

    HICON resource_icon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(fallback_resource_id),
        IMAGE_ICON,
        target_cx,
        target_cy,
        LR_DEFAULTCOLOR));
    if (resource_icon != nullptr) {
        icon_cache[cache_key] = resource_icon;
    }
    return resource_icon;
}

LRESULT CALLBACK overlay_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<const CREATESTRUCTA*>(l_param);
        auto* overlay = static_cast<InputOverlayWindow*>(create_struct->lpCreateParams);
        SetWindowLongPtrA(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    }

    auto* overlay = reinterpret_cast<InputOverlayWindow*>(GetWindowLongPtrA(window_handle, GWLP_USERDATA));

    switch (message) {
        case WM_PAINT:
            if (overlay != nullptr) {
                overlay->paint();
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window_handle);
            return 0;
        case WM_LBUTTONDOWN:
            if (overlay != nullptr && overlay->drag_mode_enabled()) {
                RECT rect{};
                GetWindowRect(window_handle, &rect);
                POINT cursor{};
                GetCursorPos(&cursor);
                overlay->begin_drag(cursor.x - rect.left, cursor.y - rect.top);
                SetCapture(window_handle);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (overlay != nullptr && overlay->is_dragging()) {
                POINT cursor{};
                GetCursorPos(&cursor);
                overlay->move_drag(cursor.x, cursor.y);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (overlay != nullptr && overlay->is_dragging()) {
                overlay->end_drag();
                ReleaseCapture();
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (overlay != nullptr && overlay->alpha_adjustment_target() != AlphaAdjustmentTarget::none) {
                overlay->add_alpha_adjustment_delta(GET_WHEEL_DELTA_WPARAM(w_param));
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }

    return DefWindowProcA(window_handle, message, w_param, l_param);
}
} // namespace
#endif

InputOverlayWindow::InputOverlayWindow() = default;

InputOverlayWindow::~InputOverlayWindow() {
    request_close();
#if defined(_WIN32)
    shutdown_gdiplus_if_started();
#endif
}

bool InputOverlayWindow::initialize() {
#if !defined(_WIN32)
    return false;
#else
    if (is_open_) {
        return true;
    }

    if (!ensure_gdiplus_started()) {
        return false;
    }

    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXA);
    window_class.lpfnWndProc = overlay_window_proc;
    window_class.hInstance = GetModuleHandleA(nullptr);
    window_class.lpszClassName = k_overlay_window_class_name;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hIcon = static_cast<HICON>(LoadImageA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(100), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    window_class.hIconSm = static_cast<HICON>(LoadImageA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(102), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (RegisterClassExA(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    HWND window_handle = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        k_overlay_window_class_name,
        "Keymera Input Overlay",
        WS_POPUP | WS_VISIBLE,
        position_.x,
        position_.y,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleA(nullptr),
        this);

    if (window_handle == nullptr) {
        return false;
    }

    if (HICON big_icon = load_app_icon_from_png(k_app_big_icon_path, 100,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)); big_icon != nullptr) {
        SendMessageA(window_handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
    }
    if (HICON small_icon = load_app_icon_from_png(k_app_small_icon_path, 102,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)); small_icon != nullptr) {
        SendMessageA(window_handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }

    window_handle_ = window_handle;
    is_open_ = true;

    apply_window_metrics();
    return true;
#endif
}

void InputOverlayWindow::enqueue_input_event(
    const std::string& key,
    const std::string& action,
    const std::string& combo,
    const std::string& repeat_kind,
    unsigned int repeat_count,
    char key_side,
    bool mouse_left_pressed,
    bool mouse_right_pressed,
    bool mouse_middle_pressed,
    bool mouse_side_1_pressed,
    bool mouse_side_2_pressed,
    bool ctrl_pressed,
    bool alt_pressed,
    bool shift_pressed,
    bool win_pressed) {
    const std::string key_upper = to_ascii_upper(key);
    const std::string action_upper = to_ascii_upper(action);
    const std::string repeat_kind_upper = to_ascii_upper(repeat_kind);
    const unsigned int event_repeat_count = repeat_count == 0 ? 1 : repeat_count;
    const bool key_is_mouse = is_mouse_overlay_key_name(key_upper);
    const bool key_is_modifier = is_keyboard_modifier_key_name(key_upper);
    const bool key_is_modifier_up = key_is_modifier && action_upper == "UP";
    const std::string previous_key_label = current_key_label_;
    const std::string previous_combo_label = current_combo_label_;
    const bool previous_mouse_left_pressed = mouse_left_pressed_;
    const bool previous_mouse_right_pressed = mouse_right_pressed_;
    const bool previous_mouse_middle_pressed = mouse_middle_pressed_;
    const bool previous_mouse_side_1_pressed = mouse_side_1_pressed_;
    const bool previous_mouse_side_2_pressed = mouse_side_2_pressed_;
    const int previous_mouse_wheel_visual_state = mouse_wheel_visual_state_;
    current_key_side_ = (key_side == 'L' || key_side == 'R') ? key_side : 'N';
    last_active_input_ts_ms_ = now_epoch_ms();

    // Always trust event-level mouse snapshot to avoid visual flicker in mixed-key chords.
    mouse_left_pressed_ = mouse_left_pressed;
    mouse_right_pressed_ = mouse_right_pressed;
    mouse_middle_pressed_ = mouse_middle_pressed;
    mouse_side_1_pressed_ = mouse_side_1_pressed;
    mouse_side_2_pressed_ = mouse_side_2_pressed;

    if (!key_upper.empty()) {
        if (action_upper == "DOWN") {
            active_hold_keys_.insert(key_upper);
            if (!key_is_mouse && !key_is_modifier) {
                active_repeat_key_ = key_upper;
                active_repeat_kind_ = repeat_kind_upper;
                active_repeat_count_ = event_repeat_count;
            }
        } else if (action_upper == "UP") {
            active_hold_keys_.erase(key_upper);
            if (active_repeat_key_ == key_upper) {
                active_repeat_key_.clear();
                active_repeat_kind_.clear();
                active_repeat_count_ = 1;
            }
        }
    }

    set_active_key_from_snapshot(
        active_hold_keys_,
        "CTRL",
        key_upper == "CTRL" ? action_upper == "DOWN" : ctrl_pressed);
    set_active_key_from_snapshot(
        active_hold_keys_,
        "ALT",
        key_upper == "ALT" ? action_upper == "DOWN" : alt_pressed);
    set_active_key_from_snapshot(
        active_hold_keys_,
        "SHIFT",
        key_upper == "SHIFT" ? action_upper == "DOWN" : shift_pressed);
    set_active_key_from_snapshot(
        active_hold_keys_,
        "WIN",
        key_upper == "WIN" ? action_upper == "DOWN" : win_pressed);

#if defined(_WIN32)
    caps_lock_active_ = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    fn_lock_active_ = (GetKeyState(VK_SCROLL) & 0x0001) != 0;
    num_lock_active_ = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
#endif

    if (key_upper == "MOUSE_WHEEL_UP") {
        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        mouse_wheel_visual_state_ = 1;
        mouse_wheel_visual_ts_ms_ = static_cast<unsigned long long>(ms.count());
    } else if (key_upper == "MOUSE_WHEEL_DOWN") {
        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        mouse_wheel_visual_state_ = -1;
        mouse_wheel_visual_ts_ms_ = static_cast<unsigned long long>(ms.count());
    }

    if (key_is_mouse) {
        current_key_label_ = key;
        current_combo_label_ = combo;
        current_key_side_ = 'N';
        current_repeat_count_ = event_repeat_count;
        current_repeat_kind_ = repeat_kind_upper;
    } else if (!key.empty()) {
        const std::string hold_combo = current_keyboard_hold_combo_label();
        if (!hold_combo.empty()) {
            const std::size_t last_separator = hold_combo.rfind('+');
            current_key_label_ = last_separator == std::string::npos
                ? hold_combo
                : hold_combo.substr(last_separator + 1);
            current_combo_label_ = last_separator == std::string::npos
                ? "NONE"
                : hold_combo;
            if (!active_repeat_key_.empty()
                && active_hold_keys_.find(active_repeat_key_) != active_hold_keys_.end()) {
                current_repeat_count_ = active_repeat_count_;
                current_repeat_kind_ = active_repeat_kind_;
            } else if (action_upper == "DOWN" && !key_is_modifier) {
                current_repeat_count_ = event_repeat_count;
                current_repeat_kind_ = repeat_kind_upper;
            } else {
                current_repeat_count_ = 1;
                current_repeat_kind_.clear();
            }
        } else if (action_upper == "DOWN") {
            current_key_label_ = key;
            current_combo_label_ = combo;
            current_repeat_count_ = event_repeat_count;
            current_repeat_kind_ = repeat_kind_upper;
        }
    }

    const bool should_add_to_history = !is_idle_placeholder_label(key)
        && !(history_ignores_mouse_buttons_ && is_mouse_overlay_key_name(key_upper));
    if (should_add_to_history) {
        history_buffer_.push(key, action, combo, repeat_kind_upper, event_repeat_count);
    }
    if (key_is_modifier_up
        && !active_hold_keys_.empty()
        && !current_key_label_.empty()
        && !is_idle_placeholder_label(current_key_label_)) {
        history_buffer_.push_state_snapshot(current_key_label_, current_combo_label_);
    }
    (void)combo;

#if defined(_WIN32)
    if (window_handle_ != nullptr) {
        const bool stable_hold_repeat = action_upper == "DOWN"
            && repeat_kind_upper == "HOLD"
            && event_repeat_count > 1
            && !key_is_mouse
            && !key_is_modifier
            && current_key_label_ == previous_key_label
            && current_combo_label_ == previous_combo_label
            && mouse_left_pressed_ == previous_mouse_left_pressed
            && mouse_right_pressed_ == previous_mouse_right_pressed
            && mouse_middle_pressed_ == previous_mouse_middle_pressed
            && mouse_side_1_pressed_ == previous_mouse_side_1_pressed
            && mouse_side_2_pressed_ == previous_mouse_side_2_pressed
            && mouse_wheel_visual_state_ == previous_mouse_wheel_visual_state;

        if (!stable_hold_repeat || !request_lightweight_repaint_if_layout_stable()) {
            apply_window_metrics();
        }
    }
#endif
}

bool InputOverlayWindow::pump_messages(const std::atomic<bool>& should_stop) {
#if !defined(_WIN32)
    return !should_stop.load();
#else
    MSG message;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) > 0) {
        if (message.message == WM_QUIT) {
            is_open_ = false;
            return false;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    if (should_stop.load()) {
        return false;
    }

    if (mouse_wheel_visual_state_ != 0) {
        const unsigned long long now_ms = now_epoch_ms();
        const bool wheel_visual_expired = now_ms >= mouse_wheel_visual_ts_ms_
            && (now_ms - mouse_wheel_visual_ts_ms_) > 160ULL;

        if (wheel_visual_expired) {
            mouse_wheel_visual_state_ = 0;
            if (window_handle_ != nullptr) {
                apply_window_metrics();
            }
        }
    }

    const bool has_active_hold = !active_hold_keys_.empty()
        || mouse_left_pressed_
        || mouse_right_pressed_
        || mouse_middle_pressed_
        || mouse_side_1_pressed_
        || mouse_side_2_pressed_;

    if (!has_active_hold && !is_idle_placeholder_label(current_key_label_)) {
        const unsigned long long now_ms = now_epoch_ms();
        const bool idle_expired = now_ms >= last_active_input_ts_ms_
            && (now_ms - last_active_input_ts_ms_) >= idle_restore_delay_ms_;

        if (idle_expired) {
            current_key_label_ = k_waiting_key_label;
            current_combo_label_.clear();
            current_repeat_count_ = 1;
            current_repeat_kind_.clear();
            current_key_side_ = 'N';

            if (window_handle_ != nullptr) {
                apply_window_metrics();
            }
        }
    }

    return is_open_;
#endif
}

void InputOverlayWindow::request_close() {
#if defined(_WIN32)
    if (window_handle_ != nullptr) {
        DestroyWindow(static_cast<HWND>(window_handle_));
        window_handle_ = nullptr;
    }
#endif
    is_open_ = false;
}

void InputOverlayWindow::set_position(int x, int y) {
    if (position_.x == x && position_.y == y) {
        return;
    }
    position_.x = x;
    position_.y = y;
    apply_window_metrics();
}

OverlayPosition InputOverlayWindow::position() const {
    return position_;
}

OverlayPosition InputOverlayWindow::window_screen_position() const {
#if defined(_WIN32)
    if (window_handle_ != nullptr) {
        RECT rect{};
        if (GetWindowRect(static_cast<HWND>(window_handle_), &rect) != 0) {
            return OverlayPosition{static_cast<int>(rect.left), static_cast<int>(rect.top)};
        }
    }
#endif
    return position_;
}

OverlaySize InputOverlayWindow::window_screen_size() const {
#if defined(_WIN32)
    if (window_handle_ != nullptr) {
        RECT rect{};
        if (GetWindowRect(static_cast<HWND>(window_handle_), &rect) != 0) {
            return OverlaySize{
                static_cast<int>(rect.right - rect.left),
                static_cast<int>(rect.bottom - rect.top)};
        }
    }
#endif
    return OverlaySize{style_.width, style_.height};
}

void InputOverlayWindow::set_size(int width, int height) {
    const int next_width = clamp_non_negative(width);
    const int next_height = clamp_non_negative(height);
    if (style_.width == next_width && style_.height == next_height) {
        return;
    }
    style_.width = next_width;
    style_.height = next_height;
    apply_window_metrics();
}

void InputOverlayWindow::set_padding(int padding) {
    const int next_padding = clamp_non_negative(padding);
    if (style_.padding == next_padding) {
        return;
    }
    style_.padding = next_padding;
    apply_window_metrics();
}

void InputOverlayWindow::set_spacing(int spacing) {
    const int next_spacing = clamp_non_negative(spacing);
    if (style_.spacing == next_spacing) {
        return;
    }
    style_.spacing = next_spacing;
    apply_window_metrics();
}

void InputOverlayWindow::set_color(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (style_.color.r == r && style_.color.g == g && style_.color.b == b) {
        return;
    }
    style_.color = OverlayColor{r, g, b};
    apply_window_metrics();
}

void InputOverlayWindow::set_opacity(std::uint8_t opacity) {
    if (style_.opacity == opacity) {
        return;
    }
    style_.opacity = opacity;
    apply_window_metrics();
}

void InputOverlayWindow::set_corner_radius(int corner_radius) {
    const int next_corner_radius = clamp_non_negative(corner_radius);
    if (style_.corner_radius == next_corner_radius) {
        return;
    }
    style_.corner_radius = next_corner_radius;
    apply_window_metrics();
}

void InputOverlayWindow::set_style(const OverlayStyle& style) {
    OverlayStyle next_style = style;
    sanitize_overlay_style(next_style);
    if (style_.width == next_style.width
        && style_.height == next_style.height
        && style_.padding == next_style.padding
        && style_.spacing == next_style.spacing
        && style_.corner_radius == next_style.corner_radius
        && style_.opacity == next_style.opacity
        && style_.color.r == next_style.color.r
        && style_.color.g == next_style.color.g
        && style_.color.b == next_style.color.b) {
        return;
    }
    style_ = next_style;
    apply_window_metrics();
}

OverlayStyle InputOverlayWindow::style() const {
    return style_;
}

void InputOverlayWindow::set_normal_key_style(const OverlayKeyStyle& style) {
    OverlayKeyStyle next_style = style;
    sanitize_overlay_key_style(next_style);
    if (normal_key_style_.size == next_style.size
        && normal_key_style_.corner_radius == next_style.corner_radius
        && normal_key_style_.opacity == next_style.opacity
        && normal_key_style_.color.r == next_style.color.r
        && normal_key_style_.color.g == next_style.color.g
        && normal_key_style_.color.b == next_style.color.b
        && normal_key_style_.font_file_path == next_style.font_file_path
        && normal_key_style_.font_bold_file_path == next_style.font_bold_file_path
        && normal_key_style_.font_bold_italic_file_path == next_style.font_bold_italic_file_path
        && normal_key_style_.letter_font_size == next_style.letter_font_size
        && normal_key_style_.function_font_size == next_style.function_font_size
        && normal_key_style_.text_horizontal_padding == next_style.text_horizontal_padding
        && normal_key_style_.font_bold == next_style.font_bold
        && normal_key_style_.font_extra_bold_strength == next_style.font_extra_bold_strength
        && normal_key_style_.font_opacity == next_style.font_opacity
        && normal_key_style_.font_color.r == next_style.font_color.r
        && normal_key_style_.font_color.g == next_style.font_color.g
        && normal_key_style_.font_color.b == next_style.font_color.b
        && normal_key_style_.text_supersample_scale == next_style.text_supersample_scale
        && normal_key_style_.mouse_icon_supersample_scale == next_style.mouse_icon_supersample_scale
        && normal_key_style_.mouse_svg_curve_segments == next_style.mouse_svg_curve_segments
        && normal_key_style_.mouse_status_panel_enabled == next_style.mouse_status_panel_enabled
        && normal_key_style_.mouse_status_panel_width == next_style.mouse_status_panel_width
        && normal_key_style_.mouse_status_panel_height == next_style.mouse_status_panel_height
        && normal_key_style_.mouse_status_panel_gap == next_style.mouse_status_panel_gap
        && normal_key_style_.mouse_status_panel_opacity == next_style.mouse_status_panel_opacity
        && normal_key_style_.mouse_status_panel_bg_color.r == next_style.mouse_status_panel_bg_color.r
        && normal_key_style_.mouse_status_panel_bg_color.g == next_style.mouse_status_panel_bg_color.g
        && normal_key_style_.mouse_status_panel_bg_color.b == next_style.mouse_status_panel_bg_color.b
        && normal_key_style_.mouse_status_panel_font_size == next_style.mouse_status_panel_font_size
        && normal_key_style_.mouse_has_side_buttons == next_style.mouse_has_side_buttons
        && normal_key_style_.mouse_status_panel_underline_thickness == next_style.mouse_status_panel_underline_thickness
        && normal_key_style_.mouse_status_panel_active_color.r == next_style.mouse_status_panel_active_color.r
        && normal_key_style_.mouse_status_panel_active_color.g == next_style.mouse_status_panel_active_color.g
        && normal_key_style_.mouse_status_panel_active_color.b == next_style.mouse_status_panel_active_color.b
        && normal_key_style_.mouse_status_panel_inactive_color.r == next_style.mouse_status_panel_inactive_color.r
        && normal_key_style_.mouse_status_panel_inactive_color.g == next_style.mouse_status_panel_inactive_color.g
        && normal_key_style_.mouse_status_panel_inactive_color.b == next_style.mouse_status_panel_inactive_color.b
        && normal_key_style_.mouse_status_panel_lr_dark_active_color.r == next_style.mouse_status_panel_lr_dark_active_color.r
        && normal_key_style_.mouse_status_panel_lr_dark_active_color.g == next_style.mouse_status_panel_lr_dark_active_color.g
        && normal_key_style_.mouse_status_panel_lr_dark_active_color.b == next_style.mouse_status_panel_lr_dark_active_color.b
        && normal_key_style_.mouse_status_panel_lr_dark_inactive_color.r == next_style.mouse_status_panel_lr_dark_inactive_color.r
        && normal_key_style_.mouse_status_panel_lr_dark_inactive_color.g == next_style.mouse_status_panel_lr_dark_inactive_color.g
        && normal_key_style_.mouse_status_panel_lr_dark_inactive_color.b == next_style.mouse_status_panel_lr_dark_inactive_color.b
        && normal_key_style_.mouse_status_panel_lr_light_active_color.r == next_style.mouse_status_panel_lr_light_active_color.r
        && normal_key_style_.mouse_status_panel_lr_light_active_color.g == next_style.mouse_status_panel_lr_light_active_color.g
        && normal_key_style_.mouse_status_panel_lr_light_active_color.b == next_style.mouse_status_panel_lr_light_active_color.b
        && normal_key_style_.mouse_status_panel_lr_light_inactive_color.r == next_style.mouse_status_panel_lr_light_inactive_color.r
        && normal_key_style_.mouse_status_panel_lr_light_inactive_color.g == next_style.mouse_status_panel_lr_light_inactive_color.g
        && normal_key_style_.mouse_status_panel_lr_light_inactive_color.b == next_style.mouse_status_panel_lr_light_inactive_color.b
        && normal_key_style_.function_text_bottom_margin == next_style.function_text_bottom_margin
        && normal_key_style_.function_key_display_mode == next_style.function_key_display_mode) {
        return;
    }
    normal_key_style_ = next_style;
    apply_window_metrics();
}

OverlayKeyStyle InputOverlayWindow::normal_key_style() const {
    return normal_key_style_;
}

void InputOverlayWindow::set_function_key_display_mode(FunctionKeyDisplayMode mode) {
    if (normal_key_style_.function_key_display_mode == mode) {
        return;
    }
    normal_key_style_.function_key_display_mode = mode;
    apply_window_metrics();
}

FunctionKeyDisplayMode InputOverlayWindow::function_key_display_mode() const {
    return normal_key_style_.function_key_display_mode;
}

void InputOverlayWindow::set_theme_mode(ThemeMode mode) {
    if (theme_mode_ == mode) {
        return;
    }
    theme_mode_ = mode;
}

ThemeMode InputOverlayWindow::theme_mode() const {
    return theme_mode_;
}

void InputOverlayWindow::set_theme_color_scheme(const ThemeColorScheme& scheme) {
    theme_color_scheme_ = scheme;
}

ThemeColorScheme InputOverlayWindow::theme_color_scheme() const {
    return theme_color_scheme_;
}

void InputOverlayWindow::set_fn_lock_active(bool active) {
    if (fn_lock_active_ == active) {
        return;
    }
    fn_lock_active_ = active;
    apply_window_metrics();
}

bool InputOverlayWindow::fn_lock_active() const {
    return fn_lock_active_;
}

void InputOverlayWindow::set_caps_lock_active(bool active) {
    if (caps_lock_active_ == active) {
        return;
    }
    caps_lock_active_ = active;
    apply_window_metrics();
}

bool InputOverlayWindow::caps_lock_active() const {
    return caps_lock_active_;
}

void InputOverlayWindow::set_idle_restore_delay_ms(unsigned long long delay_ms) {
    idle_restore_delay_ms_ = delay_ms == 0 ? 1 : delay_ms;
}

unsigned long long InputOverlayWindow::idle_restore_delay_ms() const {
    return idle_restore_delay_ms_;
}

void InputOverlayWindow::set_history_display_limit(unsigned int count) {
    if (history_buffer_.limit() == count) {
        return;
    }
    history_buffer_.set_limit(count);
    apply_window_metrics();
}

unsigned int InputOverlayWindow::history_display_limit() const {
    return history_buffer_.limit();
}

void InputOverlayWindow::set_history_ignores_mouse_buttons(bool enabled) {
    history_ignores_mouse_buttons_ = enabled;
}

bool InputOverlayWindow::history_ignores_mouse_buttons() const {
    return history_ignores_mouse_buttons_;
}

void InputOverlayWindow::set_history_item_scale(float scale) {
    const float next_scale = clamp_history_item_scale(scale);
    if (history_item_scale_ == next_scale) {
        return;
    }
    history_item_scale_ = next_scale;
    apply_window_metrics();
}

float InputOverlayWindow::history_item_scale() const {
    return history_item_scale_;
}

void InputOverlayWindow::set_history_item_opacity(std::uint8_t opacity) {
    if (history_item_opacity_ == opacity) {
        return;
    }
    history_item_opacity_ = opacity;
    apply_window_metrics();
}

std::uint8_t InputOverlayWindow::history_item_opacity() const {
    return history_item_opacity_;
}

void InputOverlayWindow::set_history_layout_direction(HistoryLayoutDirection direction) {
    if (history_layout_direction_ == direction) {
        return;
    }
    history_layout_direction_ = direction;
    apply_window_metrics();
}

HistoryLayoutDirection InputOverlayWindow::history_layout_direction() const {
    return history_layout_direction_;
}

void InputOverlayWindow::set_right_aligned_layout(bool enabled) {
    if (right_aligned_layout_ == enabled) {
        return;
    }
    right_aligned_layout_ = enabled;
    apply_window_metrics();
}

bool InputOverlayWindow::right_aligned_layout() const {
    return right_aligned_layout_;
}

void InputOverlayWindow::set_drag_mode_enabled(bool enabled) {
    if (drag_mode_enabled_ == enabled) {
        return;
    }
    drag_mode_enabled_ = enabled;
    if (!drag_mode_enabled_) {
        is_dragging_ = false;
    }
}

bool InputOverlayWindow::drag_mode_enabled() const {
    return drag_mode_enabled_;
}

void InputOverlayWindow::set_alpha_adjustment_target(AlphaAdjustmentTarget target) {
    alpha_adjustment_target_ = target;
    if (alpha_adjustment_target_ == AlphaAdjustmentTarget::none) {
        alpha_adjustment_steps_ = 0;
    }
}

AlphaAdjustmentTarget InputOverlayWindow::alpha_adjustment_target() const {
    return alpha_adjustment_target_;
}

void InputOverlayWindow::add_alpha_adjustment_delta(int wheel_delta) {
    alpha_adjustment_steps_ += wheel_delta / WHEEL_DELTA;
}

int InputOverlayWindow::consume_alpha_adjustment_steps() {
    const int steps = alpha_adjustment_steps_;
    alpha_adjustment_steps_ = 0;
    return steps;
}

void InputOverlayWindow::begin_drag(int offset_x, int offset_y) {
    if (!drag_mode_enabled_) {
        return;
    }
    is_dragging_ = true;
    drag_offset_x_ = std::max(0, offset_x);
    drag_offset_y_ = std::max(0, offset_y);
}

bool InputOverlayWindow::is_dragging() const {
    return is_dragging_;
}

void InputOverlayWindow::move_drag(int cursor_x, int cursor_y) {
    if (!is_dragging_) {
        return;
    }
    const int dragged_left = cursor_x - drag_offset_x_;
    position_.x = right_aligned_layout_
        ? (dragged_left + std::max(0, last_window_width_))
        : dragged_left;
    position_.y = cursor_y - drag_offset_y_;
    apply_window_metrics();
}

void InputOverlayWindow::end_drag() {
    if (is_dragging_) {
        drag_position_changed_ = true;
    }
    is_dragging_ = false;
}

bool InputOverlayWindow::consume_drag_position_changed() {
    const bool changed = drag_position_changed_;
    drag_position_changed_ = false;
    return changed;
}

void InputOverlayWindow::clear_history() {
    history_buffer_.clear();
    apply_window_metrics();
}

std::vector<std::string> InputOverlayWindow::history_records() const {
    return history_buffer_.export_text_lines();
}

void InputOverlayWindow::invalidate_frame_cache() {
    frame_cache_valid_ = false;
}

bool InputOverlayWindow::try_refresh_history_cache_without_layout() {
    if (!frame_cache_valid_ || !cached_frame_metrics_.has_value()) {
        return false;
    }

    const auto next_history_lines = history_buffer_.export_text_lines();
    if (next_history_lines.size() != cached_history_lines_.size()) {
        return false;
    }

    const auto& cached_metrics = *cached_frame_metrics_;
    for (const std::string& line : next_history_lines) {
        const int text_width = approximate_text_width_px(line, cached_metrics.history_font_size, false) + 24;
        if (text_width > cached_metrics.history_card_width) {
            return false;
        }
    }

    cached_history_lines_ = next_history_lines;
    return true;
}

bool InputOverlayWindow::request_lightweight_repaint_if_layout_stable() {
#if defined(_WIN32)
    if (window_handle_ == nullptr || !try_refresh_history_cache_without_layout()) {
        return false;
    }

    InvalidateRect(static_cast<HWND>(window_handle_), nullptr, FALSE);
    return true;
#else
    return false;
#endif
}

const model::OverlayFrameMetrics& InputOverlayWindow::rebuild_frame_cache() {
    if (!frame_cache_valid_ || !cached_frame_metrics_.has_value()) {
        cached_history_lines_ = history_buffer_.export_text_lines();
        cached_frame_metrics_ = build_overlay_frame_metrics(
            current_combo_label_, current_key_label_,
            mouse_left_pressed_, mouse_right_pressed_, mouse_middle_pressed_,
            mouse_side_1_pressed_, mouse_side_2_pressed_,
            mouse_wheel_visual_state_, mouse_wheel_visual_ts_ms_,
            style_, normal_key_style_,
            right_aligned_layout_,
            history_layout_direction_,
            history_item_scale_,
            cached_history_lines_);
        frame_cache_valid_ = true;
    }

    return *cached_frame_metrics_;
}

std::string InputOverlayWindow::current_keyboard_hold_combo_label() const {
    std::vector<std::string> tokens;
    tokens.reserve(active_hold_keys_.size());

    append_token_if_present(tokens, active_hold_keys_, "CTRL");
    append_token_if_present(tokens, active_hold_keys_, "ALT");
    append_token_if_present(tokens, active_hold_keys_, "SHIFT");
    append_token_if_present(tokens, active_hold_keys_, "WIN");

    std::vector<std::string> non_modifier_tokens;
    non_modifier_tokens.reserve(active_hold_keys_.size());
    for (const std::string& key_name : active_hold_keys_) {
        const std::string key_name_upper = to_ascii_upper(key_name);
        if (is_mouse_overlay_key_name(key_name_upper)
            || is_mouse_wheel_key_name(key_name_upper)
            || is_keyboard_modifier_key_name(key_name_upper)) {
            continue;
        }
        non_modifier_tokens.push_back(key_name_upper);
    }

    std::sort(non_modifier_tokens.begin(), non_modifier_tokens.end());
    tokens.insert(tokens.end(), non_modifier_tokens.begin(), non_modifier_tokens.end());

    if (tokens.empty()) {
        return "";
    }

    std::string combo = tokens[0];
    for (std::size_t index = 1; index < tokens.size(); ++index) {
        combo += "+";
        combo += tokens[index];
    }
    return combo;
}

void InputOverlayWindow::apply_window_metrics() {
#if defined(_WIN32)
    if (window_handle_ == nullptr) {
        return;
    }

    invalidate_frame_cache();
    const auto& m = rebuild_frame_cache();

    const int window_y = position_.y - (m.history_grows_up ? m.history_section_height : 0);
    int window_x = position_.x;
    if (right_aligned_layout_) {
        window_x = position_.x - m.window_width;
    }

    RECT work_rect{};
    POINT anchor_point{position_.x, position_.y};
    HMONITOR monitor = MonitorFromPoint(anchor_point, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr) {
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetMonitorInfoW(monitor, &monitor_info) != 0) {
            work_rect = monitor_info.rcWork;
        }
    }
    if (work_rect.right <= work_rect.left || work_rect.bottom <= work_rect.top) {
        work_rect.left = 0;
        work_rect.top = 0;
        work_rect.right = GetSystemMetrics(SM_CXSCREEN);
        work_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int work_left = static_cast<int>(work_rect.left);
    const int work_top = static_cast<int>(work_rect.top);
    const int work_right = static_cast<int>(work_rect.right);
    const int work_bottom = static_cast<int>(work_rect.bottom);

    const int max_x = std::max(work_left, work_right - m.window_width);
    const int max_y = std::max(work_top, work_bottom - m.window_height);

    int clamped_x = std::clamp(window_x, work_left, max_x);
    int clamped_y = std::clamp(window_y, work_top, max_y);

    // Keep right anchor visually fixed at the right edge when enabled.
    if (right_aligned_layout_) {
        const int desired_right_edge = std::clamp(position_.x, work_left, work_right);
        const int desired_left_edge = desired_right_edge - m.window_width;
        clamped_x = std::clamp(desired_left_edge, work_left, max_x);
    }

    if (last_window_x_ != clamped_x
        || last_window_y_ != clamped_y
        || last_window_width_ != m.window_width
        || last_window_height_ != m.window_height) {
        SetWindowPos(
            static_cast<HWND>(window_handle_),
            nullptr,
            clamped_x,
            clamped_y,
            m.window_width,
            m.window_height,
            SWP_NOZORDER | SWP_NOACTIVATE);
        last_window_x_ = clamped_x;
        last_window_y_ = clamped_y;
        last_window_width_ = m.window_width;
        last_window_height_ = m.window_height;
    }

    InvalidateRect(static_cast<HWND>(window_handle_), nullptr, FALSE);
#endif
}

void InputOverlayWindow::paint() {
#if defined(_WIN32)
    if (window_handle_ == nullptr) {
        return;
    }

    PAINTSTRUCT paint_struct;
    BeginPaint(static_cast<HWND>(window_handle_), &paint_struct);

    const auto& m = rebuild_frame_cache();
    const auto& history_lines = cached_history_lines_;

    const auto& keyboard_tokens      = m.keyboard_tokens;
    const bool  has_mouse_main_key   = m.has_mouse_main_key;
    const int   keyboard_row_width   = m.keyboard_row_width;
    const int   row_width            = m.row_width;
    const int   combo_spacing        = m.combo_spacing;
    const int   mouse_gap            = m.mouse_gap;
    const int   background_width     = m.background_width;
    const int   background_height    = m.background_height;
    const int   window_width         = m.window_width;
    const int   window_height        = m.window_height;
    const int   background_x         = m.background_x;
    const int   background_y         = m.background_y;
    const unsigned int history_count       = m.history_count;
    const int   history_font_size          = m.history_font_size;
    const int   history_card_height        = m.history_card_height;
    const int   history_card_spacing       = m.history_card_spacing;
    const int   history_section_height     = m.history_section_height;
    const int   history_card_width         = m.history_card_width;
    const int   history_card_x_left        = m.history_card_x;
    const int   history_left_padding       = m.history_left_padding;
    const int   history_right_padding      = m.history_right_padding;
    const int   history_gap                = m.history_gap;
    const int   radius = clamped_corner_radius(background_width, background_height, style_.corner_radius);

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = window_width;
    bitmap_info.bmiHeader.biHeight = -window_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    HDC screen_dc = GetDC(nullptr);
    HDC memory_dc = CreateCompatibleDC(screen_dc);

    void* pixels = nullptr;
    HBITMAP dib_bitmap = CreateDIBSection(memory_dc, &bitmap_info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HGDIOBJ old_bitmap = SelectObject(memory_dc, dib_bitmap);

    Graphics graphics(memory_dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.Clear(Color(0, 0, 0, 0));

    GraphicsPath background_path;
    add_rounded_rect_path(
        background_path,
        static_cast<float>(background_x),
        static_cast<float>(background_y),
        static_cast<float>(background_width),
        static_cast<float>(background_height),
        static_cast<float>(radius));

    SolidBrush background_brush(Color(style_.opacity, style_.color.r, style_.color.g, style_.color.b));
    graphics.FillPath(&background_brush, &background_path);

    const int key_height = clamp_key_size(normal_key_style_.size, background_width, background_height);
    const int combo_start_x = right_aligned_layout_
        ? (background_x + background_width - row_width - history_right_padding)
        : (background_x + ((background_width - row_width) / 2));
    int running_x = combo_start_x;
    const int primary_content_height = clamp_non_negative(style_.height);
    const int key_y = background_y + ((primary_content_height - key_height) / 2);

    static CachedFontFamilies bold_font_cache;
    static CachedFontFamilies bold_italic_font_cache;
    const CachedFontFamilies& bold_font = load_cached_font_families(normal_key_style_.font_bold_file_path, bold_font_cache);
    const CachedFontFamilies& bold_italic_font = load_cached_font_families(normal_key_style_.font_bold_italic_file_path, bold_italic_font_cache);
    const FontFamily* bold_families = bold_font.families.get();
    const int bold_count = bold_font.count;
    const FontFamily* bold_italic_families = bold_italic_font.families.get();
    const int bold_italic_count = bold_italic_font.count;

    for (std::size_t index = 0; index < keyboard_tokens.size(); ++index) {
        const std::string& key_name = keyboard_tokens[index];
        const std::string display_text = compose_key_display_text(key_name, normal_key_style_.function_key_display_mode);
        const std::wstring label_text = utf8_to_utf16(display_text);

        const std::string key_name_upper = to_ascii_upper(key_name);
        const bool is_function_key = is_function_key_name(key_name_upper);
        const int requested_font_size = is_function_key ? normal_key_style_.function_font_size : normal_key_style_.letter_font_size;
        const int font_size = clamp_non_negative(requested_font_size);

        int key_width = key_height;
        if (is_function_key) {
            const int estimated_text_width = approximate_text_width_px(
                display_text,
                font_size,
                normal_key_style_.font_bold);
            const int padded_width = estimated_text_width + (normal_key_style_.text_horizontal_padding * 2);
            if (padded_width > key_width) {
                key_width = padded_width;
            }
        }

        const int outer_radius = clamped_corner_radius(key_width, key_height, normal_key_style_.corner_radius);
        const int inner_radius = clamped_corner_radius(key_width, key_height, 2);

        int top_left_radius = outer_radius;
        int top_right_radius = outer_radius;
        int bottom_right_radius = outer_radius;
        int bottom_left_radius = outer_radius;

        if (keyboard_tokens.size() > 1) {
            if (index == 0) {
                top_right_radius = inner_radius;
                bottom_right_radius = inner_radius;
            } else if (index + 1 == keyboard_tokens.size()) {
                top_left_radius = inner_radius;
                bottom_left_radius = inner_radius;
            } else {
                top_left_radius = inner_radius;
                top_right_radius = inner_radius;
                bottom_right_radius = inner_radius;
                bottom_left_radius = inner_radius;
            }
        }

        if (top_left_radius == 0 && top_right_radius == 0 && bottom_right_radius == 0 && bottom_left_radius == 0) {
            SolidBrush key_brush(Color(normal_key_style_.opacity, normal_key_style_.color.r, normal_key_style_.color.g, normal_key_style_.color.b));
            graphics.FillRectangle(
                &key_brush,
                static_cast<float>(running_x),
                static_cast<float>(key_y),
                static_cast<float>(key_width),
                static_cast<float>(key_height));
        } else {
            GraphicsPath key_path;
            add_rounded_rect_path(
                key_path,
                static_cast<float>(running_x),
                static_cast<float>(key_y),
                static_cast<float>(key_width),
                static_cast<float>(key_height),
                static_cast<float>(top_left_radius),
                static_cast<float>(top_right_radius),
                static_cast<float>(bottom_right_radius),
                static_cast<float>(bottom_left_radius));

            SolidBrush key_brush(Color(normal_key_style_.opacity, normal_key_style_.color.r, normal_key_style_.color.g, normal_key_style_.color.b));
            graphics.FillPath(&key_brush, &key_path);
        }

        if (!label_text.empty() && font_size > 0) {
            const int text_supersample_scale = clamp_quality_scale(normal_key_style_.text_supersample_scale);
            SolidBrush text_brush(Color(normal_key_style_.font_opacity, normal_key_style_.font_color.r, normal_key_style_.font_color.g, normal_key_style_.font_color.b));

            const int supersampled_width = key_width * text_supersample_scale;
            const int supersampled_height = key_height * text_supersample_scale;
            Gdiplus::Bitmap text_bitmap(supersampled_width, supersampled_height, &graphics);
            Graphics text_graphics(&text_bitmap);
            text_graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            text_graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
            text_graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            text_graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            text_graphics.Clear(Color(0, 0, 0, 0));

            StringFormat path_format;
            path_format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);

            const FontFamily* selected_font_family = nullptr;
            int font_style = Gdiplus::FontStyleRegular;

            if (is_function_key) {
                if (bold_italic_count > 0) {
                    selected_font_family = &bold_italic_families[0];
                    font_style = Gdiplus::FontStyleRegular;
                }
            } else {
                if (bold_count > 0) {
                    selected_font_family = &bold_families[0];
                    font_style = Gdiplus::FontStyleRegular;
                }
            }

            if (selected_font_family == nullptr) {
                running_x += key_width;
                if (index + 1 < keyboard_tokens.size()) {
                    running_x += combo_spacing;
                }
                continue;
            }

            const Gdiplus::REAL em_size = static_cast<Gdiplus::REAL>(font_size * text_supersample_scale);
            GraphicsPath text_path;
            const Gdiplus::PointF text_origin(0.0f, 0.0f);
            text_path.AddString(
                label_text.c_str(),
                static_cast<INT>(label_text.size()),
                selected_font_family,
                font_style,
                em_size,
                text_origin,
                &path_format);

            RectF glyph_bounds;
            text_path.GetBounds(&glyph_bounds);

            Gdiplus::Matrix place_transform;
            const Gdiplus::REAL center_x = static_cast<Gdiplus::REAL>(supersampled_width) * 0.5f;
            const Gdiplus::REAL center_y = static_cast<Gdiplus::REAL>(supersampled_height) * 0.5f;
            const int function_bottom_margin_px = clamp_non_negative(normal_key_style_.function_text_bottom_margin);
            const float key_size_scale = static_cast<float>(key_height) / 60.0f;
            const int scaled_function_bottom_margin_px = std::max(
                1,
                static_cast<int>(std::lround(static_cast<float>(function_bottom_margin_px) * key_size_scale)));
            const Gdiplus::REAL function_key_bottom_margin = static_cast<Gdiplus::REAL>(scaled_function_bottom_margin_px * text_supersample_scale);

            if (is_function_key) {
                const Gdiplus::REAL target_bottom = static_cast<Gdiplus::REAL>(supersampled_height) - function_key_bottom_margin;
                place_transform.Translate(
                    center_x - (glyph_bounds.X + glyph_bounds.Width * 0.5f),
                    target_bottom - (glyph_bounds.Y + glyph_bounds.Height));
            } else {
                place_transform.Translate(
                    center_x - (glyph_bounds.X + glyph_bounds.Width * 0.5f),
                    center_y - (glyph_bounds.Y + glyph_bounds.Height * 0.5f));
            }

            text_path.Transform(&place_transform);
            const int extra_bold_strength = clamp_bold_strength(normal_key_style_.font_extra_bold_strength);
            if (extra_bold_strength > 0) {
                const Gdiplus::REAL stroke_width = static_cast<Gdiplus::REAL>(extra_bold_strength * text_supersample_scale);
                Gdiplus::Pen embolden_pen(
                    Color(normal_key_style_.font_opacity, normal_key_style_.font_color.r, normal_key_style_.font_color.g, normal_key_style_.font_color.b),
                    stroke_width);
                embolden_pen.SetLineJoin(Gdiplus::LineJoinRound);
                text_graphics.DrawPath(&embolden_pen, &text_path);
            }
            text_graphics.FillPath(&text_brush, &text_path);

            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.DrawImage(
                &text_bitmap,
                static_cast<Gdiplus::REAL>(running_x),
                static_cast<Gdiplus::REAL>(key_y),
                static_cast<Gdiplus::REAL>(key_width),
                static_cast<Gdiplus::REAL>(key_height));
        }

        running_x += key_width;
        if (index + 1 < keyboard_tokens.size()) {
            running_x += combo_spacing;
        }
    }

    if (keyboard_row_width > 0 && current_repeat_count_ > 1 && bold_italic_count > 0) {
        const std::string repeat_prefix = current_repeat_kind_ == "HOLD"
            ? "H"
            : (current_repeat_kind_ == "TAP" ? "T" : "");
        const std::wstring repeat_text = utf8_to_utf16(repeat_prefix + std::to_string(current_repeat_count_));
        if (!repeat_text.empty()) {
            const int repeat_font_size = std::clamp(key_height / 4, 10, 28);
            const int repeat_margin_right = 6;
            const int repeat_margin_top = 6;
            const int repeat_text_supersample_scale = clamp_quality_scale(normal_key_style_.text_supersample_scale);
            const int repeat_area_width = keyboard_row_width;
            const int repeat_area_height = key_height;
            const int repeat_area_x = combo_start_x;
            const int repeat_area_y = key_y;
            const int repeat_render_width = repeat_area_width * repeat_text_supersample_scale;
            const int repeat_render_height = repeat_area_height * repeat_text_supersample_scale;

            Gdiplus::Bitmap repeat_bitmap(repeat_render_width, repeat_render_height, &graphics);
            Graphics repeat_graphics(&repeat_bitmap);
            repeat_graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            repeat_graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
            repeat_graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            repeat_graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            repeat_graphics.Clear(Color(0, 0, 0, 0));

            StringFormat repeat_format;
            repeat_format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);

            GraphicsPath repeat_path;
            repeat_path.AddString(
                repeat_text.c_str(),
                static_cast<INT>(repeat_text.size()),
                &bold_italic_families[0],
                Gdiplus::FontStyleRegular,
                static_cast<Gdiplus::REAL>(repeat_font_size * repeat_text_supersample_scale),
                Gdiplus::PointF(0.0f, 0.0f),
                &repeat_format);

            RectF repeat_bounds;
            repeat_path.GetBounds(&repeat_bounds);

            const Gdiplus::REAL target_right = static_cast<Gdiplus::REAL>(repeat_render_width - (repeat_margin_right * repeat_text_supersample_scale));
            const Gdiplus::REAL target_top = static_cast<Gdiplus::REAL>(repeat_margin_top * repeat_text_supersample_scale);

            Gdiplus::Matrix repeat_transform;
            repeat_transform.Translate(
                target_right - (repeat_bounds.X + repeat_bounds.Width),
                target_top - repeat_bounds.Y);
            repeat_path.Transform(&repeat_transform);

            const ThemeColorCard& repeat_theme_card = (theme_mode_ == ThemeMode::light)
                ? theme_color_scheme_.light
                : theme_color_scheme_.dark;
            const OverlayColor repeat_fill_overlay_color = (theme_mode_ == ThemeMode::light)
                ? repeat_theme_card.accent_orange
                : repeat_theme_card.accent_purple;
            const Color repeat_color(
                normal_key_style_.font_opacity,
                repeat_fill_overlay_color.r,
                repeat_fill_overlay_color.g,
                repeat_fill_overlay_color.b);
            if (theme_mode_ != ThemeMode::light) {
                Gdiplus::Pen repeat_bold_pen(
                    Color(
                        normal_key_style_.font_opacity,
                        repeat_theme_card.accent_soft.r,
                        repeat_theme_card.accent_soft.g,
                        repeat_theme_card.accent_soft.b),
                    static_cast<Gdiplus::REAL>(1.2f * repeat_text_supersample_scale));
                repeat_bold_pen.SetLineJoin(Gdiplus::LineJoinRound);
                repeat_graphics.DrawPath(&repeat_bold_pen, &repeat_path);
            }
            SolidBrush repeat_brush(repeat_color);
            repeat_graphics.FillPath(&repeat_brush, &repeat_path);

            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.DrawImage(
                &repeat_bitmap,
                static_cast<Gdiplus::REAL>(repeat_area_x),
                static_cast<Gdiplus::REAL>(repeat_area_y),
                static_cast<Gdiplus::REAL>(repeat_area_width),
                static_cast<Gdiplus::REAL>(repeat_area_height));
        }
    }

    const int mouse_icon_x = keyboard_row_width > 0 ? (combo_start_x + keyboard_row_width + mouse_gap) : combo_start_x;

    if (has_mouse_main_key) {
        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        const unsigned long long now_ms = static_cast<unsigned long long>(ms.count());
        const bool wheel_visual_active = mouse_wheel_visual_state_ != 0
            && now_ms >= mouse_wheel_visual_ts_ms_
            && (now_ms - mouse_wheel_visual_ts_ms_) <= 160ULL;

        draw_mouse_main_icon(
            graphics,
            mouse_icon_x,
            key_y,
            key_height,
            key_height,
            mouse_left_pressed_,
            mouse_right_pressed_,
            mouse_middle_pressed_,
            mouse_side_1_pressed_,
            mouse_side_2_pressed_,
            normal_key_style_.mouse_has_side_buttons,
            mouse_wheel_visual_state_,
            wheel_visual_active,
            theme_mode_ == ThemeMode::light
                ? OverlayColor{26, 26, 26}
                : OverlayColor{255, 255, 255},
            theme_mode_ == ThemeMode::light
                ? OverlayColor{133, 78, 202}
                : OverlayColor{232, 154, 60},
            normal_key_style_.mouse_icon_supersample_scale,
            normal_key_style_.mouse_svg_curve_segments);
    }

    if (normal_key_style_.mouse_status_panel_enabled) {
        const int panel_width = clamp_non_negative(normal_key_style_.mouse_status_panel_width);
        const int panel_height = clamp_non_negative(normal_key_style_.mouse_status_panel_height);
        const int panel_gap = clamp_non_negative(normal_key_style_.mouse_status_panel_gap);
        const int panel_x = has_mouse_main_key
            ? (mouse_icon_x + key_height + panel_gap)
            : (keyboard_row_width > 0 ? (combo_start_x + keyboard_row_width + panel_gap) : combo_start_x);
        const int panel_y = key_y + ((key_height - panel_height) / 2);
        draw_mouse_status_panel(
            graphics,
            panel_x,
            panel_y,
            normal_key_style_,
                caps_lock_active_,
            num_lock_active_,
            current_key_side_,
            theme_mode_);
    }

    if (history_count > 0) {
        const int history_start_y = m.history_grows_up
            ? (background_y - history_section_height + history_gap)
            : (background_y + clamp_non_negative(style_.height) + history_gap);

        Font fallback_history_font(L"Segoe UI", static_cast<Gdiplus::REAL>(history_font_size), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        std::unique_ptr<Font> function_like_history_font;
        if (bold_count > 0) {
            function_like_history_font = std::make_unique<Font>(
                &bold_families[0],
                static_cast<Gdiplus::REAL>(history_font_size),
                Gdiplus::FontStyleRegular,
                Gdiplus::UnitPixel);
        }

        Font* history_font = function_like_history_font ? function_like_history_font.get() : &fallback_history_font;
        const int history_card_x = right_aligned_layout_
            ? (background_x + background_width - history_right_padding - history_card_width)
            : history_card_x_left;
        const ThemeColorCard& history_theme_card = (theme_mode_ == ThemeMode::light)
            ? theme_color_scheme_.light
            : theme_color_scheme_.dark;
        const Color history_outline_color(
            history_item_opacity_,
            history_theme_card.key_background.r,
            history_theme_card.key_background.g,
            history_theme_card.key_background.b);
        const Color history_fill_color(
            history_item_opacity_,
            history_theme_card.overlay_background.r,
            history_theme_card.overlay_background.g,
            history_theme_card.overlay_background.b);
        SolidBrush history_outline_brush(history_outline_color);
        SolidBrush history_fill_brush(history_fill_color);
        StringFormat history_format;
        history_format.SetAlignment(right_aligned_layout_ ? StringAlignmentFar : Gdiplus::StringAlignmentNear);
        history_format.SetLineAlignment(StringAlignmentCenter);
        history_format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        std::vector<std::string> lines;
        lines.reserve(history_count);
        for (unsigned int i = 0; i < history_count && i < static_cast<unsigned int>(history_lines.size()); ++i) {
            lines.push_back(history_lines[i]);
        }

        for (unsigned int i = 0; i < static_cast<unsigned int>(lines.size()); ++i) {
            const std::wstring line_text = utf8_to_utf16(lines[i]);
            const int card_y = history_start_y + (static_cast<int>(i) * (history_card_height + history_card_spacing));

            const RectF history_text_rect(
                static_cast<Gdiplus::REAL>(history_card_x),
                static_cast<Gdiplus::REAL>(card_y),
                static_cast<Gdiplus::REAL>(history_card_width),
                static_cast<Gdiplus::REAL>(history_card_height));

            const Gdiplus::REAL outline_step = 1.0f;
            const RectF history_text_rect_left(
                history_text_rect.X - outline_step,
                history_text_rect.Y,
                history_text_rect.Width,
                history_text_rect.Height);
            const RectF history_text_rect_right(
                history_text_rect.X + outline_step,
                history_text_rect.Y,
                history_text_rect.Width,
                history_text_rect.Height);
            const RectF history_text_rect_up(
                history_text_rect.X,
                history_text_rect.Y - outline_step,
                history_text_rect.Width,
                history_text_rect.Height);
            const RectF history_text_rect_down(
                history_text_rect.X,
                history_text_rect.Y + outline_step,
                history_text_rect.Width,
                history_text_rect.Height);

            graphics.DrawString(
                line_text.c_str(),
                -1,
                history_font,
                history_text_rect_left,
                &history_format,
                &history_outline_brush);
            graphics.DrawString(
                line_text.c_str(),
                -1,
                history_font,
                history_text_rect_right,
                &history_format,
                &history_outline_brush);
            graphics.DrawString(
                line_text.c_str(),
                -1,
                history_font,
                history_text_rect_up,
                &history_format,
                &history_outline_brush);
            graphics.DrawString(
                line_text.c_str(),
                -1,
                history_font,
                history_text_rect_down,
                &history_format,
                &history_outline_brush);

            graphics.DrawString(
                line_text.c_str(),
                -1,
                history_font,
                history_text_rect,
                &history_format,
                &history_fill_brush);
        }
    }

    RECT window_rect{};
    GetWindowRect(static_cast<HWND>(window_handle_), &window_rect);

    POINT dst_pos{window_rect.left, window_rect.top};
    POINT src_pos{0, 0};
    SIZE window_size{window_width, window_height};

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        static_cast<HWND>(window_handle_),
        screen_dc,
        &dst_pos,
        &window_size,
        memory_dc,
        &src_pos,
        0,
        &blend,
        ULW_ALPHA);

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(dib_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);

    EndPaint(static_cast<HWND>(window_handle_), &paint_struct);
#endif
}

} // namespace Keymera::layers::input_overlay_window

