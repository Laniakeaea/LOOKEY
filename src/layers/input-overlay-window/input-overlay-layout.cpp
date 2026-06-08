#include "input-overlay-layout.hpp"

#include <cctype>
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Keymera::layers::input_overlay_window::layout {

namespace {
bool is_f_key_pattern(const std::string& key_name_upper) {
    if (key_name_upper.size() < 2 || key_name_upper[0] != 'F') {
        return false;
    }

    for (std::size_t i = 1; i < key_name_upper.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(key_name_upper[i])) == 0) {
            return false;
        }
    }

    return true;
}

const std::unordered_map<std::string, std::string>& non_function_key_display_table() {
    static const std::unordered_map<std::string, std::string> table{
        {"SEMICOLON", ";"},
        {"EQUAL", "="},
        {"COMMA", ","},
        {"MINUS", "-"},
        {"PERIOD", "."},
        {"SLASH", "/"},
        {"BACKQUOTE", "`"},
        {"LBRACKET", "["},
        {"RBRACKET", "]"},
        {"BACKSLASH", "\\"},
        {"OEM_102", "\\"},
        {"QUOTE", "'"},
    };
    return table;
}

const std::unordered_set<std::string>& named_function_keys() {
    static const std::unordered_set<std::string> keys{
        "ESC", "TAB", "ENTER", "BACKSPACE", "DELETE", "INSERT", "HOME", "END",
        "PAGE_UP", "PAGE_DOWN", "PRINT_SCREEN", "SCROLL_LOCK", "PAUSE", "NUM_LOCK",
        "SHIFT", "CTRL", "ALT", "WIN", "SPACE", "CAPS", "UP", "DOWN", "LEFT", "RIGHT",
        "SLEEP", "BROWSER_BACK", "BROWSER_FORWARD", "BROWSER_REFRESH", "BROWSER_STOP",
        "BROWSER_SEARCH", "BROWSER_FAVORITES", "BROWSER_HOME",
        "VOLUME_MUTE", "VOLUME_DOWN", "VOLUME_UP",
        "MEDIA_NEXT", "MEDIA_PREV", "MEDIA_STOP", "MEDIA_PLAY_PAUSE",
        "LAUNCH_MAIL", "LAUNCH_MEDIA", "LAUNCH_APP_1", "LAUNCH_APP_2",
    };
    return keys;
}

const std::unordered_map<std::string, std::string>& function_key_icon_table() {
    static const std::unordered_map<std::string, std::string> table{
        {"ESC", "\xE2\x8E\x8B"},
        {"TAB", "\xE2\x86\xB9"},
        {"ENTER", "\xE2\x86\xB5"},
        {"BACKSPACE", "\xE2\x8C\xAB"},
        {"DELETE", "\xE2\x8C\xA6"},
        {"INSERT", "INS"},
        {"HOME", "HM"},
        {"END", "END"},
        {"PAGE_UP", "PGUP"},
        {"PAGE_DOWN", "PGDN"},
        {"PRINT_SCREEN", "PRT"},
        {"SCROLL_LOCK", "SCRL"},
        {"PAUSE", "PAUSE"},
        {"NUM_LOCK", "NUM"},
        {"SLEEP", "SLP"},
        {"BROWSER_BACK", "B<"},
        {"BROWSER_FORWARD", "B>"},
        {"BROWSER_REFRESH", "RFR"},
        {"BROWSER_STOP", "BST"},
        {"BROWSER_SEARCH", "SRCH"},
        {"BROWSER_FAVORITES", "FAV"},
        {"BROWSER_HOME", "BHM"},
        {"VOLUME_MUTE", "MUTE"},
        {"VOLUME_DOWN", "VOL-"},
        {"VOLUME_UP", "VOL+"},
        {"MEDIA_NEXT", "NXT"},
        {"MEDIA_PREV", "PRV"},
        {"MEDIA_STOP", "STOP"},
        {"MEDIA_PLAY_PAUSE", "PLAY"},
        {"LAUNCH_MAIL", "MAIL"},
        {"LAUNCH_MEDIA", "MEDIA"},
        {"LAUNCH_APP_1", "APP1"},
        {"LAUNCH_APP_2", "APP2"},
        {"SHIFT", "\xE2\x87\xA7"},
        {"CTRL", "\xE2\x8C\x83"},
        {"ALT", "\xE2\x8E\x87"},
        {"WIN", "\xE2\x8A\x9E"},
        {"SPACE", "\xE2\x90\xA0"},
        {"CAPS", "\xE2\x87\xAA"},
        {"UP", "\xE2\x86\x91"},
        {"DOWN", "\xE2\x86\x93"},
        {"LEFT", "\xE2\x86\x90"},
        {"RIGHT", "\xE2\x86\x92"},
    };
    return table;
}

const std::unordered_set<std::string>& mouse_overlay_key_set() {
    static const std::unordered_set<std::string> keys{
        "MOUSE_LEFT", "MOUSE_RIGHT", "MOUSE_MIDDLE", "MOUSE_SIDE_1", "MOUSE_SIDE_2",
        "MOUSE_WHEEL_UP", "MOUSE_WHEEL_DOWN", "MOUSE_WHEEL_LEFT", "MOUSE_WHEEL_RIGHT",
    };
    return keys;
}

const std::unordered_set<std::string>& mouse_wheel_key_set() {
    static const std::unordered_set<std::string> keys{
        "MOUSE_WHEEL_UP", "MOUSE_WHEEL_DOWN", "MOUSE_WHEEL_LEFT", "MOUSE_WHEEL_RIGHT",
    };
    return keys;
}
} // namespace

int clamp_non_negative(int value) {
    return value < 0 ? 0 : value;
}

int clamp_quality_scale(int value) {
    if (value < 1) {
        return 1;
    }
    return value > 8 ? 8 : value;
}

int clamp_curve_segments(int value) {
    if (value < 4) {
        return 4;
    }
    return value > 128 ? 128 : value;
}

int clamp_bold_strength(int value) {
    if (value < 0) {
        return 0;
    }
    return value > 8 ? 8 : value;
}

int clamped_corner_radius(int width, int height, int requested_radius) {
    const int min_side = (width < height) ? width : height;
    const int max_radius = min_side / 2;
    const int radius = clamp_non_negative(requested_radius);
    return radius > max_radius ? max_radius : radius;
}

int clamp_key_size(int key_size, int background_width, int background_height) {
    const int min_side = (background_width < background_height) ? background_width : background_height;
    const int clamped = clamp_non_negative(key_size);
    return clamped > min_side ? min_side : clamped;
}

int approximate_text_width_px(const std::string& text, int font_size, bool bold) {
    if (text.empty() || font_size <= 0) {
        return 0;
    }

    double width = 0.0;
    for (const char ch : text) {
        if (ch == ' ') {
            width += 0.33;
        } else if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            width += 0.56;
        } else if (std::isupper(static_cast<unsigned char>(ch)) != 0) {
            width += 0.62;
        } else if (std::islower(static_cast<unsigned char>(ch)) != 0) {
            width += 0.56;
        } else {
            width += 0.70;
        }
    }

    if (bold) {
        width *= 1.08;
    }

    return static_cast<int>(std::ceil(width * static_cast<double>(font_size)));
}

std::string to_ascii_upper(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string trim_ascii_spaces(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }

    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
        --last;
    }

    return text.substr(first, last - first);
}

std::string normalize_combo_token(std::string token) {
    token = trim_ascii_spaces(token);
    if (token.empty()) {
        return "";
    }

    const std::size_t colon_pos = token.find(':');
    if (colon_pos != std::string::npos) {
        token = token.substr(0, colon_pos);
        token = trim_ascii_spaces(token);
    }

    return token;
}

std::string display_text_for_non_function_key(const std::string& key_name_upper) {
    const auto& table = non_function_key_display_table();
    const auto it = table.find(key_name_upper);
    if (it != table.end()) {
        return it->second;
    }

    return key_name_upper;
}

bool is_function_key_name(const std::string& key_name_upper) {
    if (is_f_key_pattern(key_name_upper)) {
        return true;
    }

    return named_function_keys().find(key_name_upper) != named_function_keys().end();
}

bool is_mouse_overlay_key_name(const std::string& key_name_upper) {
    return mouse_overlay_key_set().find(key_name_upper) != mouse_overlay_key_set().end();
}

bool is_mouse_wheel_key_name(const std::string& key_name_upper) {
    return mouse_wheel_key_set().find(key_name_upper) != mouse_wheel_key_set().end();
}

std::string function_key_icon(const std::string& key_name_upper) {
    const auto& table = function_key_icon_table();
    const auto it = table.find(key_name_upper);
    if (it != table.end()) {
        return it->second;
    }

    if (is_f_key_pattern(key_name_upper)) {
        return key_name_upper;
    }

    return "";
}

std::string compose_key_display_text(const std::string& key_name, FunctionKeyDisplayMode mode) {
    if (key_name.empty()) {
        return "";
    }

    const std::string key_name_upper = to_ascii_upper(key_name);
    if (!is_function_key_name(key_name_upper)) {
        return display_text_for_non_function_key(key_name_upper);
    }

    const std::string icon = function_key_icon(key_name_upper);
    if (mode == FunctionKeyDisplayMode::key_name_only) {
        if (key_name_upper == "PRINT_SCREEN") {
            return "PRTSC";
        }
        if (key_name_upper == "PAGE_UP") {
            return "PAGE UP";
        }
        if (key_name_upper == "PAGE_DOWN") {
            return "PAGE DOWN";
        }
        if (key_name_upper == "BROWSER_BACK") {
            return "BROWSER BACK";
        }
        if (key_name_upper == "BROWSER_FORWARD") {
            return "BROWSER FORWARD";
        }
        if (key_name_upper == "BROWSER_REFRESH") {
            return "BROWSER REFRESH";
        }
        if (key_name_upper == "BROWSER_STOP") {
            return "BROWSER STOP";
        }
        if (key_name_upper == "BROWSER_SEARCH") {
            return "BROWSER SEARCH";
        }
        if (key_name_upper == "BROWSER_FAVORITES") {
            return "BROWSER FAVORITES";
        }
        if (key_name_upper == "BROWSER_HOME") {
            return "BROWSER HOME";
        }
        if (key_name_upper == "VOLUME_MUTE") {
            return "VOLUME MUTE";
        }
        if (key_name_upper == "VOLUME_DOWN") {
            return "VOLUME DOWN";
        }
        if (key_name_upper == "VOLUME_UP") {
            return "VOLUME UP";
        }
        if (key_name_upper == "MEDIA_NEXT") {
            return "MEDIA NEXT";
        }
        if (key_name_upper == "MEDIA_PREV") {
            return "MEDIA PREV";
        }
        if (key_name_upper == "MEDIA_STOP") {
            return "MEDIA STOP";
        }
        if (key_name_upper == "MEDIA_PLAY_PAUSE") {
            return "MEDIA PLAY";
        }
        if (key_name_upper == "LAUNCH_MAIL") {
            return "MAIL";
        }
        if (key_name_upper == "LAUNCH_MEDIA") {
            return "MEDIA";
        }
        if (key_name_upper == "LAUNCH_APP_1") {
            return "APP1";
        }
        if (key_name_upper == "LAUNCH_APP_2") {
            return "APP2";
        }
        return key_name_upper;
    }
    if (mode == FunctionKeyDisplayMode::icon_and_key_name) {
        return icon.empty() ? key_name_upper : (icon + " " + key_name_upper);
    }
    return icon.empty() ? key_name_upper : icon;
}

std::vector<std::string> parse_combo_tokens(const std::string& combo_text, const std::string& fallback_key) {
    const std::string source = combo_text.empty() ? fallback_key : combo_text;
    const std::string source_upper = to_ascii_upper(source);
    if (source_upper == "NONE") {
        return fallback_key.empty() ? std::vector<std::string>{} : std::vector<std::string>{fallback_key};
    }

    std::vector<std::string> tokens;
    std::string token;

    for (const char ch : source) {
        if (ch == '+') {
            const std::string normalized = normalize_combo_token(token);
            if (!normalized.empty()) {
                tokens.push_back(normalized);
            }
            token.clear();
            continue;
        }

        token.push_back(ch);
    }

    const std::string normalized = normalize_combo_token(token);
    if (!normalized.empty()) {
        tokens.push_back(normalized);
    }

    if (tokens.empty() && !fallback_key.empty()) {
        tokens.push_back(fallback_key);
    }

    return tokens;
}

int compute_combo_total_key_width(
    const std::vector<std::string>& tokens,
    const OverlayKeyStyle& key_style,
    int combo_spacing) {
    if (tokens.empty()) {
        return 0;
    }

    int total_width = 0;
    const int key_height = clamp_non_negative(key_style.size);
    const int spacing = clamp_non_negative(combo_spacing);

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        int key_width = key_height;

        const std::string key_name_upper = to_ascii_upper(tokens[i]);
        const bool is_function_key = is_function_key_name(key_name_upper);
        if (is_function_key) {
            const std::string display_text = compose_key_display_text(tokens[i], key_style.function_key_display_mode);
            const int estimated_text_width = approximate_text_width_px(
                display_text,
                clamp_non_negative(key_style.function_font_size),
                key_style.font_bold);
            const int padded_width = estimated_text_width + (clamp_non_negative(key_style.text_horizontal_padding) * 2);
            if (padded_width > key_width) {
                key_width = padded_width;
            }
        }

        total_width += key_width;
        if (i + 1 < tokens.size()) {
            total_width += spacing;
        }
    }

    return total_width;
}

int compute_background_width_for_combo(
    int combo_total_key_width,
    const OverlayStyle& overlay_style,
    const OverlayKeyStyle& key_style) {
    const int base_background_width = clamp_non_negative(overlay_style.width);
    const int base_key_size = clamp_non_negative(key_style.size);
    const int base_side_margin = ((base_background_width - base_key_size) > 0)
        ? ((base_background_width - base_key_size) / 2)
        : 0;

    const int expanded_background_width = combo_total_key_width + (base_side_margin * 2);
    return expanded_background_width > base_background_width ? expanded_background_width : base_background_width;
}

bool has_any_mouse_overlay_pressed(
    bool left_pressed,
    bool right_pressed,
    bool middle_pressed,
    bool side_1_pressed,
    bool side_2_pressed,
    int wheel_visual_state,
    unsigned long long wheel_visual_ts_ms) {
    if (left_pressed || right_pressed || middle_pressed || side_1_pressed || side_2_pressed) {
        return true;
    }

    if (wheel_visual_state == 0) {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    const unsigned long long now_ms = static_cast<unsigned long long>(ms.count());
    return now_ms >= wheel_visual_ts_ms && (now_ms - wheel_visual_ts_ms) <= 160ULL;
}

std::vector<std::string> extract_keyboard_tokens(const std::vector<std::string>& combo_tokens, bool& has_mouse_main_key) {
    has_mouse_main_key = false;
    std::vector<std::string> keyboard_tokens;
    keyboard_tokens.reserve(combo_tokens.size());

    for (const std::string& token : combo_tokens) {
        const std::string token_upper = to_ascii_upper(token);
        if (is_mouse_overlay_key_name(token_upper)) {
            has_mouse_main_key = true;
            continue;
        }

        keyboard_tokens.push_back(token);
    }

    return keyboard_tokens;
}

int compute_overlay_row_width(
    int keyboard_row_width,
    bool has_mouse_main_key,
    int mouse_icon_width,
    int mouse_gap,
    int mouse_status_panel_width,
    int mouse_status_panel_gap,
    bool mouse_status_panel_enabled) {
    int total_width = keyboard_row_width;
    if (has_mouse_main_key) {
        if (total_width > 0) {
            total_width += mouse_gap;
        }
        total_width += mouse_icon_width;
    }

    if (mouse_status_panel_enabled) {
        if (total_width > 0) {
            total_width += clamp_non_negative(mouse_status_panel_gap);
        }
        total_width += clamp_non_negative(mouse_status_panel_width);
    }

    return total_width;
}

} // namespace Keymera::layers::input_overlay_window::layout