#pragma once

#include "../input-overlay-window.hpp"
#include "../input-overlay-layout.hpp"

namespace Keymera::layers::input_overlay_window::style {

/// Clamp all numeric fields of an OverlayStyle to their legal ranges in-place.
void sanitize_overlay_style(OverlayStyle& s);

/// Clamp all numeric fields of an OverlayKeyStyle to their legal ranges in-place.
void sanitize_overlay_key_style(OverlayKeyStyle& s);

} // namespace Keymera::layers::input_overlay_window::style
