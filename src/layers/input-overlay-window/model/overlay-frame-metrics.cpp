#include "overlay-frame-metrics.hpp"

#include "../input-overlay-layout.hpp"

namespace lookey::layers::input_overlay_window::model {

using namespace layout;

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
    const std::vector<std::string>& history_lines) {

    OverlayFrameMetrics m;

    // ── Key row ──────────────────────────────────────────────────────────────
    const std::vector<std::string> combo_tokens =
        parse_combo_tokens(current_combo_label, current_key_label);

    m.keyboard_tokens = extract_keyboard_tokens(combo_tokens, m.has_mouse_main_key);
    const int scaled_compact_spacing = std::max(1, clamp_non_negative(key_style.size) / 30);
    m.combo_spacing   = scaled_compact_spacing;
    m.keyboard_row_width =
        compute_combo_total_key_width(m.keyboard_tokens, key_style, m.combo_spacing);

    m.mouse_icon_width =
        clamp_non_negative(key_style.size);
    m.mouse_gap = 8;
    m.mouse_status_panel_width =
        key_style.mouse_status_panel_enabled
            ? clamp_non_negative(key_style.mouse_status_panel_width)
            : 0;
    m.mouse_status_panel_gap =
        key_style.mouse_status_panel_enabled
            ? clamp_non_negative(key_style.mouse_status_panel_gap)
            : 0;

    m.has_mouse_main_key = m.has_mouse_main_key || has_any_mouse_overlay_pressed(
        mouse_left_pressed, mouse_right_pressed, mouse_middle_pressed,
        mouse_side_1_pressed, mouse_side_2_pressed,
        mouse_wheel_visual_state, mouse_wheel_visual_ts_ms);

    m.row_width = compute_overlay_row_width(
        m.keyboard_row_width,
        m.has_mouse_main_key,
        m.mouse_icon_width,
        m.mouse_gap,
        m.mouse_status_panel_width,
        m.mouse_status_panel_gap,
        key_style.mouse_status_panel_enabled);
    if (m.row_width <= 0) {
        m.row_width = clamp_non_negative(key_style.size);
    }

    // ── History geometry ─────────────────────────────────────────────────────
    m.history_count          = static_cast<unsigned int>(history_lines.size());
    m.history_font_size      = std::max(
        10,
        static_cast<int>(static_cast<float>(key_style.function_font_size) * history_item_scale));
    m.history_vertical_padding = 4;
    m.history_gap            = m.history_count > 0 ? 8 : 0;
    m.history_card_height    = m.history_count > 0
        ? std::max(18, m.history_font_size + (m.history_vertical_padding * 2))
        : 0;
    m.history_card_spacing   = m.history_count > 0 ? scaled_compact_spacing : 0;
    m.history_section_height = m.history_gap
        + (static_cast<int>(m.history_count) * m.history_card_height)
        + (m.history_count > 0
            ? static_cast<int>(m.history_count - 1) * m.history_card_spacing
            : 0);

    m.background_width =
        compute_background_width_for_combo(m.row_width, style, key_style);

    int history_text_max_width = 0;
    for (const std::string& line : history_lines) {
        const int w = approximate_text_width_px(line, m.history_font_size, false);
        if (w > history_text_max_width) {
            history_text_max_width = w;
        }
    }

    m.history_card_width = m.history_count > 0
        ? std::max(
            std::max(48, static_cast<int>(
                static_cast<float>(m.background_width) * history_item_scale)),
            history_text_max_width + 24)
        : 0;

    m.history_left_padding  = 10;
    m.history_right_padding = 10;
    const int history_required_width = m.history_count > 0
        ? (m.history_left_padding + m.history_card_width + m.history_right_padding)
        : 0;
    const int content_width = std::max(m.background_width, history_required_width);

    // ── Window geometry ───────────────────────────────────────────────────────
    m.background_height = clamp_non_negative(style.height);
    m.outer_margin_x    = 0;
    m.outer_margin_y    = style.padding + style.spacing;
    m.window_width      = content_width + (m.outer_margin_x * 2);
    m.window_height     = m.background_height + m.history_section_height
                          + (m.outer_margin_y * 2);
    m.background_x      = right_aligned_layout
        ? (m.outer_margin_x + (content_width - m.background_width))
        : m.outer_margin_x;
    m.history_grows_up  = (history_layout_direction == HistoryLayoutDirection::up);
    m.background_y      = m.outer_margin_y + (m.history_grows_up ? m.history_section_height : 0);
    m.history_card_x    = right_aligned_layout
        ? (m.outer_margin_x + content_width - m.history_right_padding - m.history_card_width)
        : (m.background_x + m.history_left_padding);

    return m;
}

} // namespace lookey::layers::input_overlay_window::model
