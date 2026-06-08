#include "theme-config.hpp"

namespace lookey::layers::input_overlay_window::theme_config {

const ThemePaletteConfig& palette_for_mode(ThemeMode mode) {
    static const ThemePaletteConfig dark_mode_palette{
        OverlayColor{0, 0, 0},       // overlay background
        OverlayColor{0, 0, 0},       // background
        OverlayColor{255, 255, 255}, // key background
        OverlayColor{232, 154, 60},  // accent primary (#E89A3C)
        OverlayColor{133, 78, 202},  // accent secondary (#854ECA)
        OverlayColor{43, 29, 17},    // accent muted (#2B1D11)
        OverlayColor{76, 76, 76},    // text muted (#4C4C4C)
    };

    static const ThemePaletteConfig light_mode_palette{
        OverlayColor{255, 255, 255}, // overlay background
        OverlayColor{255, 255, 255}, // background
        OverlayColor{26, 26, 26},    // key background (#1A1A1A)
        OverlayColor{133, 78, 202},  // accent primary (#854ECA)
        OverlayColor{232, 154, 60},  // accent secondary (#E89A3C)
        OverlayColor{235, 215, 250}, // accent muted (#EBD7FA)
        OverlayColor{230, 230, 230}, // text muted (#E6E6E6)
    };

    return mode == ThemeMode::dark ? dark_mode_palette : light_mode_palette;
}

void apply_palette_to_styles(
    ThemeMode mode,
    OverlayStyle& overlay_style,
    OverlayKeyStyle& key_style,
    OverlayColor& background) {
    const ThemePaletteConfig& palette = palette_for_mode(mode);

    overlay_style.color = palette.overlay_background;
    key_style.color = palette.key_background;
    key_style.font_color = palette.text_muted;

    key_style.mouse_status_panel_active_color = palette.accent_primary;
    key_style.mouse_status_panel_inactive_color = palette.accent_muted;
    key_style.mouse_status_panel_lr_dark_active_color = palette.key_background;
    key_style.mouse_status_panel_lr_dark_inactive_color = palette.text_muted;
    key_style.mouse_status_panel_lr_light_active_color = palette.key_background;
    key_style.mouse_status_panel_lr_light_inactive_color = palette.text_muted;

    // Keep the right-side vertical container colorless.
    key_style.mouse_status_panel_opacity = 0;
    key_style.mouse_status_panel_bg_color = OverlayColor{0, 0, 0};

    background = palette.overlay_background;
}

} // namespace lookey::layers::input_overlay_window::theme_config
