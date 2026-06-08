#pragma once

#include "model/history-buffer.hpp"
#include "model/overlay-frame-metrics.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace lookey::layers::input_overlay_window {

struct OverlayPosition {
    int x{24};
    int y{24};
};

struct OverlaySize {
    int width{0};
    int height{0};
};

struct OverlayColor {
    std::uint8_t r{204};
    std::uint8_t g{204};
    std::uint8_t b{204};
};

enum class FunctionKeyDisplayMode {
    key_name_only,
    icon_and_key_name,
    icon_only,
};

enum class ThemeMode {
    dark,
    light,
};

enum class HistoryLayoutDirection {
    down,
    up,
};

enum class AlphaAdjustmentTarget {
    none,
    current_display,
    history,
};

struct ThemeColorCard {
    OverlayColor overlay_background{0, 0, 0};
    OverlayColor key_foreground{255, 255, 255};
    OverlayColor key_background{255, 255, 255};
    OverlayColor text_primary{26, 26, 26};
    OverlayColor accent_orange{232, 154, 60};
    OverlayColor accent_purple{133, 78, 202};
    OverlayColor accent_muted{43, 29, 17};
    OverlayColor accent_soft{235, 215, 250};
    OverlayColor text_muted{76, 76, 76};
    OverlayColor surface_soft{230, 230, 230};
};

struct ThemeColorScheme {
    ThemeColorCard dark{};
    ThemeColorCard light{
        OverlayColor{255, 255, 255},
        OverlayColor{0, 0, 0},
        OverlayColor{26, 26, 26},
        OverlayColor{255, 255, 255},
        OverlayColor{232, 154, 60},
        OverlayColor{133, 78, 202},
        OverlayColor{230, 230, 230},
        OverlayColor{235, 215, 250},
        OverlayColor{76, 76, 76},
        OverlayColor{43, 29, 17},
    };
};

struct OverlayStyle {
    int width{100};
    int height{100};
    int padding{20};
    int spacing{0};
    int corner_radius{20};
    std::uint8_t opacity{255};
    OverlayColor color{};
};

struct OverlayKeyStyle {
    int size{60};
    int corner_radius{10};
    std::uint8_t opacity{255};
    OverlayColor color{255, 255, 255};
    std::string font_file_path{"assets/fonts/UbuntuMono/UbuntuMonoNerdFont-Regular.ttf"};
    std::string font_bold_file_path{"assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-Bold.ttf"};
    std::string font_bold_italic_file_path{"assets/fonts/UbuntuMono/UbuntuMonoNerdFontMono-BoldItalic.ttf"};
    int letter_font_size{32};
    int function_font_size{16};
    int text_horizontal_padding{12};
    bool font_bold{true};
    int font_extra_bold_strength{0};
    std::uint8_t font_opacity{255};
    OverlayColor font_color{0, 0, 0};
    int text_supersample_scale{4};
    int mouse_icon_supersample_scale{4};
    int mouse_svg_curve_segments{60};
    bool mouse_status_panel_enabled{true};
    int mouse_status_panel_width{20};
    int mouse_status_panel_height{60};
    int mouse_status_panel_gap{4};
    std::uint8_t mouse_status_panel_opacity{255};
    OverlayColor mouse_status_panel_bg_color{0, 0, 0};
    int mouse_status_panel_font_size{16};
    bool mouse_has_side_buttons{true};
    int mouse_status_panel_underline_thickness{0};
    OverlayColor mouse_status_panel_active_color{232, 154, 60};
    OverlayColor mouse_status_panel_inactive_color{43, 29, 17};
    OverlayColor mouse_status_panel_lr_dark_active_color{255, 255, 255};
    OverlayColor mouse_status_panel_lr_dark_inactive_color{26, 26, 26};
    OverlayColor mouse_status_panel_lr_light_active_color{26, 26, 26};
    OverlayColor mouse_status_panel_lr_light_inactive_color{204, 204, 204};
    int function_text_bottom_margin{5};
    FunctionKeyDisplayMode function_key_display_mode{FunctionKeyDisplayMode::key_name_only};
};

class InputOverlayWindow {
public:
    InputOverlayWindow();
    ~InputOverlayWindow();

    bool initialize();
    void enqueue_input_event(
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
        bool win_pressed);
    bool pump_messages(const std::atomic<bool>& should_stop);
    void request_close();
    void paint();

    void set_position(int x, int y);
    OverlayPosition position() const;
    OverlayPosition window_screen_position() const;
    OverlaySize window_screen_size() const;

    void set_size(int width, int height);
    void set_padding(int padding);
    void set_spacing(int spacing);
    void set_color(std::uint8_t r, std::uint8_t g, std::uint8_t b);
    void set_opacity(std::uint8_t opacity);
    void set_corner_radius(int corner_radius);
    void set_style(const OverlayStyle& style);
    OverlayStyle style() const;

    void set_normal_key_style(const OverlayKeyStyle& style);
    OverlayKeyStyle normal_key_style() const;

    void set_function_key_display_mode(FunctionKeyDisplayMode mode);
    FunctionKeyDisplayMode function_key_display_mode() const;

    void set_theme_mode(ThemeMode mode);
    ThemeMode theme_mode() const;
    void set_theme_color_scheme(const ThemeColorScheme& scheme);
    ThemeColorScheme theme_color_scheme() const;
    void set_fn_lock_active(bool active);
    bool fn_lock_active() const;
    void set_caps_lock_active(bool active);
    bool caps_lock_active() const;

    void set_idle_restore_delay_ms(unsigned long long delay_ms);
    unsigned long long idle_restore_delay_ms() const;

    void set_history_display_limit(unsigned int count);
    unsigned int history_display_limit() const;
    void set_history_ignores_mouse_buttons(bool enabled);
    bool history_ignores_mouse_buttons() const;
    void set_history_item_scale(float scale);
    float history_item_scale() const;
    void set_history_item_opacity(std::uint8_t opacity);
    std::uint8_t history_item_opacity() const;
    void set_history_layout_direction(HistoryLayoutDirection direction);
    HistoryLayoutDirection history_layout_direction() const;
    void set_right_aligned_layout(bool enabled);
    bool right_aligned_layout() const;
    void set_drag_mode_enabled(bool enabled);
    bool drag_mode_enabled() const;
    void set_alpha_adjustment_target(AlphaAdjustmentTarget target);
    AlphaAdjustmentTarget alpha_adjustment_target() const;
    void add_alpha_adjustment_delta(int wheel_delta);
    int consume_alpha_adjustment_steps();
    void begin_drag(int offset_x, int offset_y);
    bool is_dragging() const;
    void move_drag(int cursor_x, int cursor_y);
    void end_drag();
    bool consume_drag_position_changed();
    void clear_history();
    std::vector<std::string> history_records() const;

private:
    void apply_window_metrics();
    void invalidate_frame_cache();
    bool try_refresh_history_cache_without_layout();
    bool request_lightweight_repaint_if_layout_stable();
    const model::OverlayFrameMetrics& rebuild_frame_cache();
    std::string current_keyboard_hold_combo_label() const;

    // HistoryRecord lives in model/history-buffer.hpp
    using HistoryRecord = model::HistoryRecord;

    void* window_handle_{nullptr};
    bool is_open_{false};
    OverlayPosition position_{};
    OverlayStyle style_{};
    OverlayKeyStyle normal_key_style_{};
    ThemeMode theme_mode_{ThemeMode::dark};
    ThemeColorScheme theme_color_scheme_{};
    std::string current_key_label_{"󰟢"};
    std::string current_combo_label_{};
    std::string current_repeat_kind_{};
    unsigned int current_repeat_count_{1};
    std::string active_repeat_key_{};
    std::string active_repeat_kind_{};
    unsigned int active_repeat_count_{1};
    bool mouse_left_pressed_{false};
    bool mouse_right_pressed_{false};
    bool mouse_middle_pressed_{false};
    bool mouse_side_1_pressed_{false};
    bool mouse_side_2_pressed_{false};
    bool fn_lock_active_{false};
    bool caps_lock_active_{false};
    bool num_lock_active_{false};
    char current_key_side_{'N'};
    int mouse_wheel_visual_state_{0};
    unsigned long long mouse_wheel_visual_ts_ms_{0};
    unsigned long long idle_restore_delay_ms_{450};
    unsigned long long last_active_input_ts_ms_{0};
    float history_item_scale_{0.8f};
    std::uint8_t history_item_opacity_{255};
    bool history_ignores_mouse_buttons_{false};
    HistoryLayoutDirection history_layout_direction_{HistoryLayoutDirection::down};
    bool right_aligned_layout_{false};
    bool drag_mode_enabled_{false};
    AlphaAdjustmentTarget alpha_adjustment_target_{AlphaAdjustmentTarget::none};
    int alpha_adjustment_steps_{0};
    bool is_dragging_{false};
    bool drag_position_changed_{false};
    int drag_offset_x_{0};
    int drag_offset_y_{0};
    int last_window_x_{-32768};
    int last_window_y_{-32768};
    int last_window_width_{-1};
    int last_window_height_{-1};
    bool frame_cache_valid_{false};
    std::optional<model::OverlayFrameMetrics> cached_frame_metrics_{};
    std::vector<std::string> cached_history_lines_{};
    model::HistoryBuffer history_buffer_{};
    std::unordered_set<std::string> active_hold_keys_{};
};

} // namespace lookey::layers::input_overlay_window
