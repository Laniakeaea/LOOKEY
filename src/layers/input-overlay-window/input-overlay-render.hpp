#pragma once

#include "input-overlay-window.hpp"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <gdiplus.h>
#endif

namespace lookey::layers::input_overlay_window::render {

#if defined(_WIN32)
bool ensure_gdiplus_started();
void shutdown_gdiplus_if_started();

std::wstring utf8_to_utf16(const std::string& text);
bool load_font_collection_from_file(const std::string& font_file_path, Gdiplus::PrivateFontCollection& font_collection);

void add_rounded_rect_path(
    Gdiplus::GraphicsPath& path,
    float x,
    float y,
    float width,
    float height,
    float top_left_radius,
    float top_right_radius,
    float bottom_right_radius,
    float bottom_left_radius);

void add_rounded_rect_path(
    Gdiplus::GraphicsPath& path,
    float x,
    float y,
    float width,
    float height,
    float radius);

void draw_mouse_main_icon(
    Gdiplus::Graphics& graphics,
    int x,
    int y,
    int width,
    int height,
    bool left_pressed,
    bool right_pressed,
    bool middle_pressed,
    bool side_1_pressed,
    bool side_2_pressed,
    bool has_side_buttons,
    int wheel_visual_state,
    bool wheel_visual_active,
    const OverlayColor& inactive_color,
    const OverlayColor& active_color,
    int icon_supersample_scale,
    int cubic_segments);

void draw_mouse_status_panel(
    Gdiplus::Graphics& graphics,
    int x,
    int y,
    const OverlayKeyStyle& key_style,
    bool caps_lock_active,
    bool num_lock_active,
    char key_side,
    ThemeMode theme_mode);
#endif

} // namespace lookey::layers::input_overlay_window::render
