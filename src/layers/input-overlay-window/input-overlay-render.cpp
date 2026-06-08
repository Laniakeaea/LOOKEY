#include "input-overlay-render.hpp"

#include "../../common/resource/embedded-assets.hpp"
#include "input-overlay-layout.hpp"
#include "input-overlay-theme.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace lookey::layers::input_overlay_window::render {

#if defined(_WIN32)
namespace {
using Gdiplus::Color;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::PrivateFontCollection;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;
using Gdiplus::SmoothingModeAntiAlias;
using lookey::common::resource::find_embedded_asset;

ULONG_PTR g_gdiplus_token = 0;

struct SvgPathSpec {
    std::string d;
    bool even_odd_fill{false};
};

struct MouseMainSvgAssets {
    std::vector<SvgPathSpec> left;
    std::vector<SvgPathSpec> left_without_side_button;
    std::vector<SvgPathSpec> right;
    std::vector<SvgPathSpec> mid;
    std::vector<SvgPathSpec> mid_up;
    std::vector<SvgPathSpec> mid_down;
    std::vector<SvgPathSpec> side_up;
    std::vector<SvgPathSpec> side_down;
};

std::string read_text_file(const std::filesystem::path& file_path) {
    const std::string generic_path = file_path.generic_string();
    const std::size_t asset_pos = generic_path.find("assets/");
    if (asset_pos != std::string::npos) {
        const std::string asset_path = generic_path.substr(asset_pos);
        if (const auto embedded = find_embedded_asset(asset_path); embedded.has_value()) {
            return std::string(
                reinterpret_cast<const char*>(embedded->data),
                reinterpret_cast<const char*>(embedded->data) + embedded->size);
        }
    }

    std::ifstream input(file_path, std::ios::in | std::ios::binary);
    if (!input) {
        return "";
    }

    std::string content;
    input.seekg(0, std::ios::end);
    const std::streampos size = input.tellg();
    if (size > 0) {
        content.resize(static_cast<std::size_t>(size));
        input.seekg(0, std::ios::beg);
        input.read(content.data(), static_cast<std::streamsize>(content.size()));
    }
    return content;
}

std::string extract_attribute(const std::string& tag_text, const std::string& attribute_name) {
    const std::string key = attribute_name + "=\"";
    const std::size_t key_pos = tag_text.find(key);
    if (key_pos == std::string::npos) {
        return "";
    }

    const std::size_t value_start = key_pos + key.size();
    const std::size_t value_end = tag_text.find('"', value_start);
    if (value_end == std::string::npos) {
        return "";
    }

    return tag_text.substr(value_start, value_end - value_start);
}

std::vector<SvgPathSpec> load_svg_path_specs(const std::filesystem::path& file_path) {
    const std::string content = read_text_file(file_path);
    std::vector<SvgPathSpec> specs;
    if (content.empty()) {
        return specs;
    }

    std::size_t cursor = 0;
    while (true) {
        const std::size_t path_start = content.find("<path", cursor);
        if (path_start == std::string::npos) {
            break;
        }

        const std::size_t path_end = content.find('>', path_start);
        if (path_end == std::string::npos) {
            break;
        }

        const std::string tag = content.substr(path_start, path_end - path_start + 1);
        const std::string d_attr = extract_attribute(tag, "d");
        if (!d_attr.empty()) {
            const std::string fill_rule = layout::to_ascii_upper(extract_attribute(tag, "fill-rule"));
            specs.push_back(SvgPathSpec{d_attr, fill_rule == "EVENODD"});
        }

        cursor = path_end + 1;
    }

    return specs;
}

void skip_svg_separators(const std::string& data, std::size_t& pos) {
    while (pos < data.size()) {
        const char ch = data[pos];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == ',') {
            ++pos;
            continue;
        }
        break;
    }
}

bool read_svg_number(const std::string& data, std::size_t& pos, float& value) {
    skip_svg_separators(data, pos);
    if (pos >= data.size()) {
        return false;
    }

    const std::size_t start = pos;
    if (data[pos] == '+' || data[pos] == '-') {
        ++pos;
    }

    bool has_digits = false;
    while (pos < data.size() && std::isdigit(static_cast<unsigned char>(data[pos])) != 0) {
        has_digits = true;
        ++pos;
    }

    if (pos < data.size() && data[pos] == '.') {
        ++pos;
        while (pos < data.size() && std::isdigit(static_cast<unsigned char>(data[pos])) != 0) {
            has_digits = true;
            ++pos;
        }
    }

    if (!has_digits) {
        pos = start;
        return false;
    }

    if (pos < data.size() && (data[pos] == 'e' || data[pos] == 'E')) {
        std::size_t exp_pos = pos + 1;
        if (exp_pos < data.size() && (data[exp_pos] == '+' || data[exp_pos] == '-')) {
            ++exp_pos;
        }
        bool exp_digits = false;
        while (exp_pos < data.size() && std::isdigit(static_cast<unsigned char>(data[exp_pos])) != 0) {
            exp_digits = true;
            ++exp_pos;
        }
        if (exp_digits) {
            pos = exp_pos;
        }
    }

    value = std::strtof(data.c_str() + start, nullptr);
    return true;
}

void add_svg_cubic_as_polyline(
    GraphicsPath& path,
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    int segment_count,
    float scale_x,
    float scale_y,
    float offset_x,
    float offset_y) {
    const int k_segments = layout::clamp_curve_segments(segment_count);
    float prev_sx = offset_x + (x0 * scale_x);
    float prev_sy = offset_y + (y0 * scale_y);
    for (int i = 1; i <= k_segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_segments);
        const float u = 1.0f - t;
        const float px =
            (u * u * u * x0) +
            (3.0f * u * u * t * x1) +
            (3.0f * u * t * t * x2) +
            (t * t * t * x3);
        const float py =
            (u * u * u * y0) +
            (3.0f * u * u * t * y1) +
            (3.0f * u * t * t * y2) +
            (t * t * t * y3);

        const float sx = offset_x + (px * scale_x);
        const float sy = offset_y + (py * scale_y);
        path.AddLine(prev_sx, prev_sy, sx, sy);
        prev_sx = sx;
        prev_sy = sy;
    }
}

void build_svg_path(
    GraphicsPath& path,
    const SvgPathSpec& spec,
    int cubic_segments,
    float scale_x,
    float scale_y,
    float offset_x,
    float offset_y) {
    const std::string& d = spec.d;
    std::size_t pos = 0;
    char command = 0;
    float current_x = 0.0f;
    float current_y = 0.0f;
    float start_x = 0.0f;
    float start_y = 0.0f;

    auto map_x = [&](float x) { return offset_x + (x * scale_x); };
    auto map_y = [&](float y) { return offset_y + (y * scale_y); };

    while (pos < d.size()) {
        skip_svg_separators(d, pos);
        if (pos >= d.size()) {
            break;
        }

        if (std::isalpha(static_cast<unsigned char>(d[pos])) != 0) {
            command = d[pos];
            ++pos;
            if (command == 'Z' || command == 'z') {
                path.CloseFigure();
                current_x = start_x;
                current_y = start_y;
            }
            continue;
        }

        if (command == 0) {
            ++pos;
            continue;
        }

        if (command == 'M' || command == 'm') {
            float x = 0.0f;
            float y = 0.0f;
            if (!read_svg_number(d, pos, x) || !read_svg_number(d, pos, y)) {
                break;
            }

            if (command == 'm') {
                x += current_x;
                y += current_y;
            }

            current_x = x;
            current_y = y;
            start_x = x;
            start_y = y;
            path.StartFigure();

            while (true) {
                std::size_t probe = pos;
                skip_svg_separators(d, probe);
                if (probe >= d.size() || std::isalpha(static_cast<unsigned char>(d[probe])) != 0) {
                    break;
                }

                float lx = 0.0f;
                float ly = 0.0f;
                if (!read_svg_number(d, pos, lx) || !read_svg_number(d, pos, ly)) {
                    break;
                }
                if (command == 'm') {
                    lx += current_x;
                    ly += current_y;
                }

                path.AddLine(map_x(current_x), map_y(current_y), map_x(lx), map_y(ly));
                current_x = lx;
                current_y = ly;
            }
            continue;
        }

        if (command == 'L' || command == 'l') {
            float x = 0.0f;
            float y = 0.0f;
            if (!read_svg_number(d, pos, x) || !read_svg_number(d, pos, y)) {
                break;
            }
            if (command == 'l') {
                x += current_x;
                y += current_y;
            }
            path.AddLine(map_x(current_x), map_y(current_y), map_x(x), map_y(y));
            current_x = x;
            current_y = y;
            continue;
        }

        if (command == 'H' || command == 'h') {
            float x = 0.0f;
            if (!read_svg_number(d, pos, x)) {
                break;
            }
            if (command == 'h') {
                x += current_x;
            }
            path.AddLine(map_x(current_x), map_y(current_y), map_x(x), map_y(current_y));
            current_x = x;
            continue;
        }

        if (command == 'V' || command == 'v') {
            float y = 0.0f;
            if (!read_svg_number(d, pos, y)) {
                break;
            }
            if (command == 'v') {
                y += current_y;
            }
            path.AddLine(map_x(current_x), map_y(current_y), map_x(current_x), map_y(y));
            current_y = y;
            continue;
        }

        if (command == 'C' || command == 'c') {
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x3 = 0.0f;
            float y3 = 0.0f;
            if (!read_svg_number(d, pos, x1) || !read_svg_number(d, pos, y1) ||
                !read_svg_number(d, pos, x2) || !read_svg_number(d, pos, y2) ||
                !read_svg_number(d, pos, x3) || !read_svg_number(d, pos, y3)) {
                break;
            }

            if (command == 'c') {
                x1 += current_x;
                y1 += current_y;
                x2 += current_x;
                y2 += current_y;
                x3 += current_x;
                y3 += current_y;
            }

            path.AddLine(map_x(current_x), map_y(current_y), map_x(current_x), map_y(current_y));
            add_svg_cubic_as_polyline(
                path,
                current_x,
                current_y,
                x1,
                y1,
                x2,
                y2,
                x3,
                y3,
                cubic_segments,
                scale_x,
                scale_y,
                offset_x,
                offset_y);

            current_x = x3;
            current_y = y3;
            continue;
        }

        ++pos;
    }
}

void draw_svg_specs(
    Graphics& graphics,
    const std::vector<SvgPathSpec>& specs,
    int cubic_segments,
    float x,
    float y,
    float width,
    float height,
    const Color& color) {
    if (specs.empty()) {
        return;
    }

    const float scale_x = width / 60.0f;
    const float scale_y = height / 60.0f;
    SolidBrush brush(color);

    for (const SvgPathSpec& spec : specs) {
        GraphicsPath path(spec.even_odd_fill ? Gdiplus::FillModeAlternate : Gdiplus::FillModeWinding);
        build_svg_path(path, spec, cubic_segments, scale_x, scale_y, x, y);
        graphics.FillPath(&brush, &path);
    }
}

MouseMainSvgAssets load_mouse_main_svg_assets() {
    const std::filesystem::path base_dir = std::filesystem::absolute("assets/icons/mouse_key_main_parts");
    MouseMainSvgAssets assets;
    assets.left = load_svg_path_specs(base_dir / "mouse_key_left.svg");
    assets.left_without_side_button = load_svg_path_specs(base_dir / "mouse_key_left_without_side_button.svg");
    assets.right = load_svg_path_specs(base_dir / "mouse_key_right.svg");
    assets.mid = load_svg_path_specs(base_dir / "mouse_key_mid.svg");
    assets.mid_up = load_svg_path_specs(base_dir / "mouse_key_mid_up.svg");
    assets.mid_down = load_svg_path_specs(base_dir / "mouse_key_mid_down.svg");
    assets.side_up = load_svg_path_specs(base_dir / "mouse_key_side_up.svg");
    assets.side_down = load_svg_path_specs(base_dir / "mouse_key_side_down.svg");
    return assets;
}

const MouseMainSvgAssets& mouse_main_svg_assets() {
    static const MouseMainSvgAssets assets = load_mouse_main_svg_assets();
    return assets;
}

} // namespace

bool ensure_gdiplus_started() {
    if (g_gdiplus_token != 0) {
        return true;
    }

    Gdiplus::GdiplusStartupInput startup_input;
    const Gdiplus::Status status = Gdiplus::GdiplusStartup(&g_gdiplus_token, &startup_input, nullptr);
    return status == Gdiplus::Ok;
}

void shutdown_gdiplus_if_started() {
    if (g_gdiplus_token != 0) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }
}

std::wstring utf8_to_utf16(const std::string& text) {
    if (text.empty()) {
        return L"";
    }

    const int required_size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (required_size <= 1) {
        return L"";
    }

    std::wstring wide_text(static_cast<std::size_t>(required_size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide_text.data(), required_size);
    wide_text.pop_back();
    return wide_text;
}

bool load_font_collection_from_file(const std::string& font_file_path, PrivateFontCollection& font_collection) {
    if (font_file_path.empty()) {
        return false;
    }

    std::string generic_path = std::filesystem::path(font_file_path).generic_string();
    const std::size_t asset_pos = generic_path.find("assets/");
    if (asset_pos != std::string::npos) {
        generic_path = generic_path.substr(asset_pos);
    }
    if (const auto embedded = find_embedded_asset(generic_path); embedded.has_value()) {
        if (font_collection.AddMemoryFont(embedded->data, static_cast<INT>(embedded->size)) != Gdiplus::Ok) {
            return false;
        }
        return font_collection.GetFamilyCount() > 0;
    }

    const std::filesystem::path absolute_path = std::filesystem::absolute(std::filesystem::path(font_file_path));
    const std::wstring wide_path = absolute_path.wstring();

    if (font_collection.AddFontFile(wide_path.c_str()) != Gdiplus::Ok) {
        return false;
    }

    return font_collection.GetFamilyCount() > 0;
}

void add_rounded_rect_path(
    GraphicsPath& path,
    float x,
    float y,
    float width,
    float height,
    float top_left_radius,
    float top_right_radius,
    float bottom_right_radius,
    float bottom_left_radius) {
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    const float right = x + width;
    const float bottom = y + height;

    const float tl_raw = top_left_radius < 0.0f ? 0.0f : top_left_radius;
    const float tr_raw = top_right_radius < 0.0f ? 0.0f : top_right_radius;
    const float br_raw = bottom_right_radius < 0.0f ? 0.0f : bottom_right_radius;
    const float bl_raw = bottom_left_radius < 0.0f ? 0.0f : bottom_left_radius;

    constexpr float k_squircle_exponent = 4.5f;
    constexpr int k_corner_segments = 60;
    // Superellipse corners look visually tighter than circular arcs at the same numeric radius.
    // Apply gain to better match perceived size while keeping geometry safe.
    constexpr float k_visual_radius_gain = 1.55f;
    const float max_visual_radius = (width < height ? width : height) * 0.5f;
    const float tl = std::min(tl_raw * k_visual_radius_gain, max_visual_radius);
    const float tr = std::min(tr_raw * k_visual_radius_gain, max_visual_radius);
    const float br = std::min(br_raw * k_visual_radius_gain, max_visual_radius);
    const float bl = std::min(bl_raw * k_visual_radius_gain, max_visual_radius);
    const auto squircle_y = [](float unit_x) {
        const float clamped_x = unit_x < 0.0f ? 0.0f : (unit_x > 1.0f ? 1.0f : unit_x);
        const float x_pow = std::pow(clamped_x, k_squircle_exponent);
        const float rem = 1.0f - x_pow;
        return std::pow(rem < 0.0f ? 0.0f : rem, 1.0f / k_squircle_exponent);
    };

    const auto add_top_right_corner = [&](float cx, float cy, float radius) {
        float prev_x = cx;
        float prev_y = cy - radius;
        for (int i = 1; i <= k_corner_segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(k_corner_segments);
            const float unit_x = t;
            const float unit_y = squircle_y(unit_x);
            const float px = cx + (unit_x * radius);
            const float py = cy - (unit_y * radius);
            path.AddLine(
                prev_x,
                prev_y,
                px,
                py);
            prev_x = px;
            prev_y = py;
        }
    };

    const auto add_bottom_right_corner = [&](float cx, float cy, float radius) {
        float prev_x = cx + radius;
        float prev_y = cy;
        for (int i = 1; i <= k_corner_segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(k_corner_segments);
            const float unit_x = 1.0f - t;
            const float unit_y = squircle_y(unit_x);
            const float px = cx + (unit_x * radius);
            const float py = cy + (unit_y * radius);
            path.AddLine(
                prev_x,
                prev_y,
                px,
                py);
            prev_x = px;
            prev_y = py;
        }
    };

    const auto add_bottom_left_corner = [&](float cx, float cy, float radius) {
        float prev_x = cx;
        float prev_y = cy + radius;
        for (int i = 1; i <= k_corner_segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(k_corner_segments);
            const float unit_x = t;
            const float unit_y = squircle_y(unit_x);
            const float px = cx - (unit_x * radius);
            const float py = cy + (unit_y * radius);
            path.AddLine(
                prev_x,
                prev_y,
                px,
                py);
            prev_x = px;
            prev_y = py;
        }
    };

    const auto add_top_left_corner = [&](float cx, float cy, float radius) {
        float prev_x = cx - radius;
        float prev_y = cy;
        for (int i = 1; i <= k_corner_segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(k_corner_segments);
            const float unit_x = 1.0f - t;
            const float unit_y = squircle_y(unit_x);
            const float px = cx - (unit_x * radius);
            const float py = cy - (unit_y * radius);
            path.AddLine(
                prev_x,
                prev_y,
                px,
                py);
            prev_x = px;
            prev_y = py;
        }
    };

    path.StartFigure();
    path.AddLine(x + tl, y, right - tr, y);

    if (tr > 0.0f) {
        add_top_right_corner(right - tr, y + tr, tr);
    }

    path.AddLine(right, y + tr, right, bottom - br);

    if (br > 0.0f) {
        add_bottom_right_corner(right - br, bottom - br, br);
    }

    path.AddLine(right - br, bottom, x + bl, bottom);

    if (bl > 0.0f) {
        add_bottom_left_corner(x + bl, bottom - bl, bl);
    }

    path.AddLine(x, bottom - bl, x, y + tl);

    if (tl > 0.0f) {
        add_top_left_corner(x + tl, y + tl, tl);
    }

    path.CloseFigure();
}

void add_rounded_rect_path(GraphicsPath& path, float x, float y, float width, float height, float radius) {
    add_rounded_rect_path(path, x, y, width, height, radius, radius, radius, radius);
}

void draw_mouse_main_icon(
    Graphics& graphics,
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
    int cubic_segments) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const int render_scale = layout::clamp_quality_scale(icon_supersample_scale);
    const int curve_segments = layout::clamp_curve_segments(cubic_segments);
    const int render_width = width * render_scale;
    const int render_height = height * render_scale;

    Gdiplus::Bitmap icon_bitmap(render_width, render_height, &graphics);
    Graphics icon_graphics(&icon_bitmap);
    icon_graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    icon_graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    icon_graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    icon_graphics.Clear(Color(0, 0, 0, 0));

    const MouseMainSvgAssets& assets = mouse_main_svg_assets();
    const Color inactive_brush_color(255, inactive_color.r, inactive_color.g, inactive_color.b);
    const Color active_brush_color(255, active_color.r, active_color.g, active_color.b);
    const Color left_color = left_pressed ? active_brush_color : inactive_brush_color;
    const Color right_color = right_pressed ? active_brush_color : inactive_brush_color;
    const Color middle_color = middle_pressed ? active_brush_color : inactive_brush_color;

    const std::vector<SvgPathSpec>& left_specs =
        (!has_side_buttons && !assets.left_without_side_button.empty())
            ? assets.left_without_side_button
            : assets.left;
    draw_svg_specs(icon_graphics, left_specs, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), left_color);
    draw_svg_specs(icon_graphics, assets.right, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), right_color);
    if (has_side_buttons) {
        const Color side_up_color = side_2_pressed ? active_brush_color : inactive_brush_color;
        const Color side_down_color = side_1_pressed ? active_brush_color : inactive_brush_color;
        draw_svg_specs(icon_graphics, assets.side_up, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), side_up_color);
        draw_svg_specs(icon_graphics, assets.side_down, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), side_down_color);
    }

    if (middle_pressed) {
        draw_svg_specs(icon_graphics, assets.mid, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), middle_color);
    } else if (wheel_visual_active && wheel_visual_state > 0) {
        draw_svg_specs(icon_graphics, assets.mid_up, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), active_brush_color);
    } else if (wheel_visual_active && wheel_visual_state < 0) {
        draw_svg_specs(icon_graphics, assets.mid_down, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), active_brush_color);
    } else {
        draw_svg_specs(icon_graphics, assets.mid, curve_segments, 0.0f, 0.0f, static_cast<float>(render_width), static_cast<float>(render_height), inactive_brush_color);
    }

    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(
        &icon_bitmap,
        static_cast<Gdiplus::REAL>(x),
        static_cast<Gdiplus::REAL>(y),
        static_cast<Gdiplus::REAL>(width),
        static_cast<Gdiplus::REAL>(height));
}

void draw_mouse_status_panel(
    Graphics& graphics,
    int x,
    int y,
    const OverlayKeyStyle& key_style,
    bool caps_lock_active,
    bool num_lock_active,
    char key_side,
    ThemeMode theme_mode) {
    const int panel_width = layout::clamp_non_negative(key_style.mouse_status_panel_width);
    const int panel_height = layout::clamp_non_negative(key_style.mouse_status_panel_height);
    if (panel_width <= 0 || panel_height <= 0) {
        return;
    }

    const int font_size = layout::clamp_non_negative(key_style.mouse_status_panel_font_size);
    if (font_size <= 0) {
        return;
    }

    const int panel_scale = layout::clamp_quality_scale(key_style.text_supersample_scale < 2 ? 2 : key_style.text_supersample_scale);
    const int render_width = panel_width * panel_scale;
    const int render_height = panel_height * panel_scale;

    Gdiplus::Bitmap panel_bitmap(render_width, render_height, &graphics);
    Graphics panel_graphics(&panel_bitmap);
    panel_graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    panel_graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    panel_graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    panel_graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    panel_graphics.Clear(Color(0, 0, 0, 0));

    SolidBrush panel_brush(theme::make_color(key_style.mouse_status_panel_opacity, key_style.mouse_status_panel_bg_color));
    panel_graphics.FillRectangle(
        &panel_brush,
        0.0f,
        0.0f,
        static_cast<Gdiplus::REAL>(render_width),
        static_cast<Gdiplus::REAL>(render_height));

    PrivateFontCollection panel_font_collection;
    std::unique_ptr<FontFamily[]> panel_families;
    int panel_family_count = 0;
    if (load_font_collection_from_file(key_style.font_bold_file_path, panel_font_collection)) {
        const int family_count = panel_font_collection.GetFamilyCount();
        if (family_count > 0) {
            panel_families = std::make_unique<FontFamily[]>(family_count);
            panel_font_collection.GetFamilies(family_count, panel_families.get(), &panel_family_count);
        }
    }

    if (panel_family_count <= 0) {
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(
            &panel_bitmap,
            static_cast<Gdiplus::REAL>(x),
            static_cast<Gdiplus::REAL>(y),
            static_cast<Gdiplus::REAL>(panel_width),
            static_cast<Gdiplus::REAL>(panel_height));
        return;
    }

    const int row_gap = std::max(1, panel_scale);
    const int available_height = render_height - (row_gap * 2);
    const int row_height = available_height / 3;
    const int cl_y = 0;
    const int nl_y = cl_y + row_height + row_gap;
    const int lr_y = nl_y + row_height + row_gap;
    const int underline_thickness = layout::clamp_non_negative(key_style.mouse_status_panel_underline_thickness) * panel_scale;
    const int underline_y = cl_y + row_height;

    if (row_height <= 0 || cl_y < 0 || nl_y < 0 || lr_y < 0 || (lr_y + row_height) > render_height) {
        return;
    }

    const Gdiplus::REAL scaled_font_size = static_cast<Gdiplus::REAL>(font_size * panel_scale);
    const Gdiplus::REAL max_row_font_size = static_cast<Gdiplus::REAL>(row_height) * 0.78f;
    const Gdiplus::REAL draw_font_size = std::max<Gdiplus::REAL>(8.0f, std::min(scaled_font_size, max_row_font_size));

    const Color cl_color = theme::make_color(255, caps_lock_active ? key_style.mouse_status_panel_active_color : key_style.mouse_status_panel_inactive_color);
    const Color nl_color = theme::make_color(255, num_lock_active ? key_style.mouse_status_panel_active_color : key_style.mouse_status_panel_inactive_color);
    const theme::MouseStatusPanelThemeColors lr_colors = theme::resolve_mouse_status_panel_lr_colors(key_style, theme_mode);

    const auto draw_centered_label = [&](const wchar_t* label, int rect_y, int rect_h, const Color& color) {
        if (label == nullptr || rect_h <= 0) {
            return;
        }

        Gdiplus::StringFormat path_format;
        path_format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);

        GraphicsPath text_path;
        text_path.AddString(
            label,
            -1,
            &panel_families[0],
            Gdiplus::FontStyleRegular,
            draw_font_size,
            Gdiplus::PointF(0.0f, 0.0f),
            &path_format);

        RectF glyph_bounds;
        text_path.GetBounds(&glyph_bounds);

        const Gdiplus::REAL center_x = static_cast<Gdiplus::REAL>(render_width) * 0.5f;
        const Gdiplus::REAL center_y = static_cast<Gdiplus::REAL>(rect_y) + (static_cast<Gdiplus::REAL>(rect_h) * 0.5f);

        Gdiplus::Matrix place_transform;
        place_transform.Translate(
            center_x - (glyph_bounds.X + glyph_bounds.Width * 0.5f),
            center_y - (glyph_bounds.Y + glyph_bounds.Height * 0.5f));
        text_path.Transform(&place_transform);

        SolidBrush text_brush(color);
        panel_graphics.FillPath(&text_brush, &text_path);
    };

    draw_centered_label(L"CL", cl_y, row_height, cl_color);
    draw_centered_label(L"NL", nl_y, row_height, nl_color);

    const auto draw_compact_lr = [&](int rect_y, int rect_h, char active_side) {
        if (rect_h <= 0) {
            return;
        }

        Gdiplus::StringFormat path_format;
        path_format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);

        GraphicsPath lr_path;
        lr_path.AddString(
            L"LR",
            -1,
            &panel_families[0],
            Gdiplus::FontStyleRegular,
            draw_font_size,
            Gdiplus::PointF(0.0f, 0.0f),
            &path_format);

        RectF lr_bounds;
        lr_path.GetBounds(&lr_bounds);

        const Gdiplus::REAL center_x = static_cast<Gdiplus::REAL>(render_width) * 0.5f;
        const Gdiplus::REAL center_y = static_cast<Gdiplus::REAL>(rect_y) + (static_cast<Gdiplus::REAL>(rect_h) * 0.5f);

        Gdiplus::Matrix lr_transform;
        lr_transform.Translate(
            center_x - (lr_bounds.X + lr_bounds.Width * 0.5f),
            center_y - (lr_bounds.Y + lr_bounds.Height * 0.5f));
        lr_path.Transform(&lr_transform);

        SolidBrush inactive_brush(lr_colors.lr_inactive);
        panel_graphics.FillPath(&inactive_brush, &lr_path);

        if (active_side != 'L' && active_side != 'R') {
            return;
        }

        const wchar_t* active_label = active_side == 'L' ? L"L" : L"R";
        GraphicsPath active_path;
        active_path.AddString(
            active_label,
            -1,
            &panel_families[0],
            Gdiplus::FontStyleRegular,
            draw_font_size,
            Gdiplus::PointF(0.0f, 0.0f),
            &path_format);

        RectF active_bounds;
        active_path.GetBounds(&active_bounds);

        GraphicsPath l_path;
        l_path.AddString(
            L"L",
            -1,
            &panel_families[0],
            Gdiplus::FontStyleRegular,
            draw_font_size,
            Gdiplus::PointF(0.0f, 0.0f),
            &path_format);
        RectF l_bounds;
        l_path.GetBounds(&l_bounds);

        GraphicsPath r_path;
        r_path.AddString(
            L"R",
            -1,
            &panel_families[0],
            Gdiplus::FontStyleRegular,
            draw_font_size,
            Gdiplus::PointF(0.0f, 0.0f),
            &path_format);
        RectF r_bounds;
        r_path.GetBounds(&r_bounds);

        const Gdiplus::REAL full_left = center_x - (lr_bounds.Width * 0.5f);
        const Gdiplus::REAL full_right = center_x + (lr_bounds.Width * 0.5f);
        const Gdiplus::REAL target_center_x = active_side == 'L'
            ? (full_left + (l_bounds.Width * 0.5f))
            : (full_right - (r_bounds.Width * 0.5f));

        Gdiplus::Matrix active_transform;
        active_transform.Translate(
            target_center_x - (active_bounds.X + active_bounds.Width * 0.5f),
            center_y - (active_bounds.Y + active_bounds.Height * 0.5f));
        active_path.Transform(&active_transform);

        SolidBrush active_brush(lr_colors.lr_active);
        panel_graphics.FillPath(&active_brush, &active_path);
    };

    draw_compact_lr(lr_y, row_height, key_side);

    if (underline_thickness > 0) {
        Gdiplus::Pen underline_pen(cl_color, static_cast<Gdiplus::REAL>(underline_thickness));
        const Gdiplus::REAL line_y = static_cast<Gdiplus::REAL>(underline_y);
        panel_graphics.DrawLine(
            &underline_pen,
            static_cast<Gdiplus::REAL>(2 * panel_scale),
            line_y,
            static_cast<Gdiplus::REAL>(render_width - (2 * panel_scale)),
            line_y);
    }

    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(
        &panel_bitmap,
        static_cast<Gdiplus::REAL>(x),
        static_cast<Gdiplus::REAL>(y),
        static_cast<Gdiplus::REAL>(panel_width),
        static_cast<Gdiplus::REAL>(panel_height));
}
#endif

} // namespace lookey::layers::input_overlay_window::render
