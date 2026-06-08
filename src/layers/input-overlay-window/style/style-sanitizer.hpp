#pragma once

#include "../input-overlay-window.hpp"
#include "../input-overlay-layout.hpp"

namespace lookey::layers::input_overlay_window::style {

/// Clamp all numeric fields of an OverlayStyle to their legal ranges in-place.
void sanitize_overlay_style(OverlayStyle& s);

/// Clamp all numeric fields of an OverlayKeyStyle to their legal ranges in-place.
void sanitize_overlay_key_style(OverlayKeyStyle& s);

} // namespace lookey::layers::input_overlay_window::style
