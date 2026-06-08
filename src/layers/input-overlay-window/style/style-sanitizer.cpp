#include "style-sanitizer.hpp"

#include "../input-overlay-layout.hpp"

namespace lookey::layers::input_overlay_window::style {

using namespace layout;

void sanitize_overlay_style(OverlayStyle& s) {
    s.width         = clamp_non_negative(s.width);
    s.height        = clamp_non_negative(s.height);
    s.padding       = clamp_non_negative(s.padding);
    s.spacing       = clamp_non_negative(s.spacing);
    s.corner_radius = clamp_non_negative(s.corner_radius);
}

void sanitize_overlay_key_style(OverlayKeyStyle& s) {
    s.size                              = clamp_non_negative(s.size);
    s.corner_radius                     = clamp_non_negative(s.corner_radius);
    s.letter_font_size                  = clamp_non_negative(s.letter_font_size);
    s.function_font_size                = clamp_non_negative(s.function_font_size);
    s.text_horizontal_padding           = clamp_non_negative(s.text_horizontal_padding);
    s.text_supersample_scale            = clamp_quality_scale(s.text_supersample_scale);
    s.mouse_icon_supersample_scale      = clamp_quality_scale(s.mouse_icon_supersample_scale);
    s.mouse_svg_curve_segments          = clamp_curve_segments(s.mouse_svg_curve_segments);
    s.mouse_status_panel_width          = clamp_non_negative(s.mouse_status_panel_width);
    s.mouse_status_panel_height         = clamp_non_negative(s.mouse_status_panel_height);
    s.mouse_status_panel_gap            = clamp_non_negative(s.mouse_status_panel_gap);
    s.mouse_status_panel_font_size      = clamp_non_negative(s.mouse_status_panel_font_size);
    s.mouse_status_panel_underline_thickness = clamp_non_negative(s.mouse_status_panel_underline_thickness);
    s.function_text_bottom_margin       = clamp_non_negative(s.function_text_bottom_margin);
    s.font_extra_bold_strength          = clamp_bold_strength(s.font_extra_bold_strength);
}

} // namespace lookey::layers::input_overlay_window::style
