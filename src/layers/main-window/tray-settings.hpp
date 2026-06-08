#pragma once

#include <string>

namespace Keymera::layers::main_window::tray_settings {

enum class ThemeSetting {
    dark,
    light,
};

enum class DisplayAnchor {
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

enum class HistoryDirection {
    down,
    up,
};

struct OverlaySettings {
    int width{80};
    int height{80};
    int padding{10};
    int spacing{10};
    int corner_radius{20};
    int opacity{255};
    int idle_restore_delay_ms{450};
    int history_limit{6};
    float history_item_scale{0.8f};
    int history_item_opacity{255};

    bool operator==(const OverlaySettings&) const = default;
};

struct InputSettings {
    int key_size{60};
    int key_corner_radius{10};
    int letter_font_size{36};
    int function_font_size{20};
    int text_horizontal_padding{12};
    int font_opacity{255};
    int font_extra_bold_strength{0};
    int mouse_status_panel_width{20};
    int mouse_status_panel_height{60};
    int mouse_status_panel_gap{4};
    int mouse_status_panel_opacity{255};
    int mouse_status_panel_font_size{16};
    bool mouse_has_side_buttons{true};
    bool history_ignores_mouse_buttons{false};

    bool operator==(const InputSettings&) const = default;
};

struct GeneralSettings {
    ThemeSetting theme{ThemeSetting::dark};
    std::string locale{"en-US"};
    bool start_with_windows{false};
    int scale_percent{100};
    DisplayAnchor display_anchor{DisplayAnchor::top_left};
    bool drag_mode_enabled{false};
    bool adjust_current_alpha_enabled{false};
    bool adjust_history_alpha_enabled{false};
    bool custom_position_enabled{false};
    int custom_position_x{24};
    int custom_position_y{24};
    int display_distance_px{24};
    HistoryDirection history_direction{HistoryDirection::down};
    OverlaySettings overlay{};
    InputSettings input{};
    unsigned long long version{0};

    bool operator==(const GeneralSettings&) const = default;
};

} // namespace Keymera::layers::main_window::tray_settings
