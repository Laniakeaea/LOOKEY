#pragma once

#include <string>
#include <vector>

namespace Keymera::layers::input_overlay_window {

struct OverlayStyle;
struct OverlayKeyStyle;
enum class HistoryLayoutDirection;

namespace model {

/// Snapshot of all geometric quantities needed for both SetWindowPos and paint.
/// Built once per render frame; consumed by apply_window_metrics and paint.
struct OverlayFrameMetrics {
    // Key row
    std::vector<std::string> keyboard_tokens;
    bool     has_mouse_main_key{false};
    int      keyboard_row_width{0};
    int      row_width{0};          // keyboard + optional mouse icon + panel
    int      combo_spacing{2};
    int      mouse_gap{8};
    int      mouse_icon_width{0};
    int      mouse_status_panel_width{0};
    int      mouse_status_panel_gap{0};

    // Background / window
    int      background_width{0};
    int      background_height{0};
    int      outer_margin_x{0};
    int      outer_margin_y{0};
    int      window_width{0};
    int      window_height{0};
    int      background_x{0};
    int      background_y{0};
    bool     history_grows_up{false};

    // History
    unsigned int history_count{0};
    int      history_font_size{0};
    int      history_vertical_padding{4};
    int      history_gap{0};
    int      history_card_height{0};
    int      history_card_spacing{0};
    int      history_section_height{0};
    int      history_card_width{0};
    int      history_card_x{0};
    int      history_left_padding{10};
    int      history_right_padding{10};
};

/// Build a complete OverlayFrameMetrics from the overlay's current state.
/// history_lines: pre-formatted strings for width estimation (newest-first).
OverlayFrameMetrics build_overlay_frame_metrics(
    const std::string&            current_combo_label,
    const std::string&            current_key_label,
    bool                          mouse_left_pressed,
    bool                          mouse_right_pressed,
    bool                          mouse_middle_pressed,
    bool                          mouse_side_1_pressed,
    bool                          mouse_side_2_pressed,
    int                           mouse_wheel_visual_state,
    unsigned long long            mouse_wheel_visual_ts_ms,
    const OverlayStyle&           style,
    const OverlayKeyStyle&        key_style,
    bool                          right_aligned_layout,
    HistoryLayoutDirection        history_layout_direction,
    float                         history_item_scale,
    const std::vector<std::string>& history_lines);

} // namespace model
} // namespace Keymera::layers::input_overlay_window
