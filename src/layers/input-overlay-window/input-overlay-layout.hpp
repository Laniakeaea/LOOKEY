#pragma once

#include "input-overlay-window.hpp"

#include <string>
#include <vector>

namespace Keymera::layers::input_overlay_window::layout {

int clamp_non_negative(int value);
int clamp_quality_scale(int value);
int clamp_curve_segments(int value);
int clamp_bold_strength(int value);
int clamped_corner_radius(int width, int height, int requested_radius);
int clamp_key_size(int key_size, int background_width, int background_height);
int approximate_text_width_px(const std::string& text, int font_size, bool bold);

std::string to_ascii_upper(std::string text);
bool is_function_key_name(const std::string& key_name_upper);
bool is_mouse_overlay_key_name(const std::string& key_name_upper);
bool is_mouse_wheel_key_name(const std::string& key_name_upper);
std::string compose_key_display_text(const std::string& key_name, FunctionKeyDisplayMode mode);

std::vector<std::string> parse_combo_tokens(const std::string& combo_text, const std::string& fallback_key);
std::vector<std::string> extract_keyboard_tokens(const std::vector<std::string>& combo_tokens, bool& has_mouse_main_key);

int compute_combo_total_key_width(
    const std::vector<std::string>& tokens,
    const OverlayKeyStyle& key_style,
    int combo_spacing);

int compute_background_width_for_combo(
    int combo_total_key_width,
    const OverlayStyle& overlay_style,
    const OverlayKeyStyle& key_style);

bool has_any_mouse_overlay_pressed(
    bool left_pressed,
    bool right_pressed,
    bool middle_pressed,
    bool side_1_pressed,
    bool side_2_pressed,
    int wheel_visual_state,
    unsigned long long wheel_visual_ts_ms);

int compute_overlay_row_width(
    int keyboard_row_width,
    bool has_mouse_main_key,
    int mouse_icon_width,
    int mouse_gap,
    int mouse_status_panel_width,
    int mouse_status_panel_gap,
    bool mouse_status_panel_enabled);

} // namespace Keymera::layers::input_overlay_window::layout
