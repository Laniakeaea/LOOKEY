#pragma once

#include "../input-overlay-window.hpp"

namespace Keymera::layers::input_overlay_window::theme_config {

struct ThemePaletteConfig {
    OverlayColor overlay_background;
    OverlayColor background;
    OverlayColor key_background;
    OverlayColor accent_primary;
    OverlayColor accent_secondary;
    OverlayColor accent_muted;
    OverlayColor text_muted;
};

const ThemePaletteConfig& palette_for_mode(ThemeMode mode);

void apply_palette_to_styles(
    ThemeMode mode,
    OverlayStyle& overlay_style,
    OverlayKeyStyle& key_style,
    OverlayColor& background);

} // namespace Keymera::layers::input_overlay_window::theme_config
