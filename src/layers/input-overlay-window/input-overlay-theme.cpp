#include "input-overlay-theme.hpp"

namespace Keymera::layers::input_overlay_window::theme {

#if defined(_WIN32)
Gdiplus::Color make_color(std::uint8_t opacity, const OverlayColor& color) {
    return Gdiplus::Color(opacity, color.r, color.g, color.b);
}

MouseStatusPanelThemeColors resolve_mouse_status_panel_lr_colors(const OverlayKeyStyle& key_style, ThemeMode theme_mode) {
    const OverlayColor active_color = (theme_mode == ThemeMode::light)
        ? key_style.mouse_status_panel_lr_light_active_color
        : key_style.mouse_status_panel_lr_dark_active_color;
    const OverlayColor inactive_color = (theme_mode == ThemeMode::light)
        ? key_style.mouse_status_panel_lr_light_inactive_color
        : key_style.mouse_status_panel_lr_dark_inactive_color;

    return MouseStatusPanelThemeColors{
        make_color(255, active_color),
        make_color(255, inactive_color)};
}
#endif

} // namespace Keymera::layers::input_overlay_window::theme
