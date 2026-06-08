#pragma once

#include "input-overlay-window.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <gdiplus.h>
#endif

namespace lookey::layers::input_overlay_window::theme {

#if defined(_WIN32)
struct MouseStatusPanelThemeColors {
    Gdiplus::Color lr_active;
    Gdiplus::Color lr_inactive;
};

Gdiplus::Color make_color(std::uint8_t opacity, const OverlayColor& color);
MouseStatusPanelThemeColors resolve_mouse_status_panel_lr_colors(const OverlayKeyStyle& key_style, ThemeMode theme_mode);
#endif

} // namespace lookey::layers::input_overlay_window::theme
