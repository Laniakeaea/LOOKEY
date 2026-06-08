#include "history-buffer.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Keymera::layers::input_overlay_window::model {

namespace {

std::string to_upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])) != 0) {
        ++a;
    }
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])) != 0) {
        --b;
    }
    return s.substr(a, b - a);
}

std::string title_case(std::string s) {
    bool new_word = true;
    for (char& c : s) {
        if (c == '_') { c = ' '; new_word = true; continue; }
        if (c == ' ')            { new_word = true; continue; }
        c = new_word
            ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
            : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        new_word = false;
    }
    return s;
}

const std::unordered_map<std::string, std::string>& readable_table() {
    static const std::unordered_map<std::string, std::string> t{
        {"CTRL",             "Ctrl"},
        {"ALT",              "Alt"},
        {"SHIFT",            "Shift"},
        {"WIN",              "Win"},
        {"ESC",              "Esc"},
        {"TAB",              "Tab"},
        {"ENTER",            "Enter"},
        {"SPACE",            "Space"},
        {"BACKSPACE",        "Backspace"},
        {"CAPS",             "Caps"},
        {"PAGE_UP",          "Page Up"},
        {"PAGE_DOWN",        "Page Down"},
        {"PRINT_SCREEN",     "Print Screen"},
        {"SCROLL_LOCK",      "Scroll Lock"},
        {"NUM_LOCK",         "Num Lock"},
        {"SLEEP",            "Sleep"},
        {"BROWSER_BACK",     "Browser Back"},
        {"BROWSER_FORWARD",  "Browser Forward"},
        {"BROWSER_REFRESH",  "Browser Refresh"},
        {"BROWSER_STOP",     "Browser Stop"},
        {"BROWSER_SEARCH",   "Browser Search"},
        {"BROWSER_FAVORITES","Browser Favorites"},
        {"BROWSER_HOME",     "Browser Home"},
        {"VOLUME_MUTE",      "Volume Mute"},
        {"VOLUME_DOWN",      "Volume Down"},
        {"VOLUME_UP",        "Volume Up"},
        {"MEDIA_NEXT",       "Media Next"},
        {"MEDIA_PREV",       "Media Prev"},
        {"MEDIA_STOP",       "Media Stop"},
        {"MEDIA_PLAY_PAUSE", "Media Play"},
        {"LAUNCH_MAIL",      "Mail"},
        {"LAUNCH_MEDIA",     "Media"},
        {"LAUNCH_APP_1",     "App1"},
        {"LAUNCH_APP_2",     "App2"},
        {"MOUSE_LEFT",       "Mouse Left"},
        {"MOUSE_RIGHT",      "Mouse Right"},
        {"MOUSE_MIDDLE",     "Mouse Middle"},
        {"MOUSE_SIDE_1",     "Mouse Side 1"},
        {"MOUSE_SIDE_2",     "Mouse Side 2"},
        {"MOUSE_WHEEL_UP",   "Wheel Up"},
        {"MOUSE_WHEEL_DOWN", "Wheel Down"},
        {"MOUSE_WHEEL_LEFT", "Wheel Left"},
        {"MOUSE_WHEEL_RIGHT","Wheel Right"},
    };
    return t;
}

} // namespace

// ── HistoryBuffer ─────────────────────────────────────────────────────────────

void HistoryBuffer::set_limit(unsigned int limit) {
    limit_ = limit;
    if (limit_ == 0) {
        records_.clear();
        pending_singleton_.reset();
        return;
    }
    while (records_.size() > limit_) {
        records_.pop_front();
    }
}

bool HistoryBuffer::push(const std::string& key,
                         const std::string& action,
                         const std::string& combo,
                         const std::string& repeat_kind,
                         unsigned int       repeat_count) {
    if (limit_ == 0) {
        pending_singleton_.reset();
        return false;
    }

    const unsigned int rc = repeat_count == 0 ? 1 : repeat_count;
    HistoryRecord rec{key, action, combo, to_upper(repeat_kind), rc};

    if (!should_record(key, action)) {
        const std::string action_upper = to_upper(action);
        if (action_upper == "UP" && pending_singleton_.has_value()) {
            const bool same_display = display_identity(*pending_singleton_) == display_identity(rec);
            const bool same_key = to_upper(pending_singleton_->key) == to_upper(key);
            if (same_display || same_key) {
                return flush_pending_record();
            }
        }
        return false;
    }

    if (is_scroll_action(action)) {
        flush_pending_record();
        return append_record(rec);
    }

    if (rc <= 1) {
        if (pending_singleton_.has_value()
            && display_identity(*pending_singleton_) == display_identity(rec)) {
            pending_singleton_ = rec;
            return false;
        }

        flush_pending_record();
        pending_singleton_ = rec;
        return false;
    }

    if (pending_singleton_.has_value()) {
        if (should_merge(*pending_singleton_, rec)) {
            if (pending_singleton_->key != rec.key
                || pending_singleton_->action != rec.action
                || pending_singleton_->combo != rec.combo) {
                rec.repeat_count += pending_singleton_->repeat_count;
            } else {
                rec.repeat_count = std::max(pending_singleton_->repeat_count, rec.repeat_count);
            }
            pending_singleton_.reset();
        } else {
            flush_pending_record();
        }
    }

    return append_record(rec);
}

bool HistoryBuffer::push_state_snapshot(const std::string& key,
                                        const std::string& combo) {
    if (limit_ == 0 || key.empty()) {
        pending_singleton_.reset();
        return false;
    }

    HistoryRecord rec{key, "STATE", combo, "", 1};
    if (pending_singleton_.has_value()
        && display_identity(*pending_singleton_) == display_identity(rec)) {
        pending_singleton_ = rec;
        return false;
    }

    flush_pending_record();
    return append_record(rec);
}

void HistoryBuffer::clear() {
    records_.clear();
    pending_singleton_.reset();
}

std::vector<std::string> HistoryBuffer::export_text_lines() const {
    std::vector<std::string> lines;
    lines.reserve(records_.size());
    for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
        lines.push_back(format_line(*it));
    }
    return lines;
}

// ── private statics ───────────────────────────────────────────────────────────

bool HistoryBuffer::should_record(const std::string& key, const std::string& action) {
    if (key.empty()) {
        return false;
    }
    const std::string a = to_upper(action);
    return a == "DOWN"
        || a == "SCROLL_UP"
        || a == "SCROLL_DOWN"
        || a == "SCROLL_LEFT"
        || a == "SCROLL_RIGHT";
}

bool HistoryBuffer::is_scroll_action(const std::string& action) {
    const std::string a = to_upper(action);
    return a == "SCROLL_UP" || a == "SCROLL_DOWN"
        || a == "SCROLL_LEFT" || a == "SCROLL_RIGHT";
}

std::string HistoryBuffer::display_identity(const HistoryRecord& r) {
    return !r.combo.empty() && to_upper(r.combo) != "NONE"
        ? readable_key_sequence(r.combo)
        : readable_key_sequence(r.key);
}

bool HistoryBuffer::should_merge(const HistoryRecord& last,
                                 const HistoryRecord& incoming) {
    const std::string last_display = display_identity(last);
    const std::string incoming_display = display_identity(incoming);
    const std::string last_repeat_kind = to_upper(last.repeat_kind);
    const std::string incoming_repeat_kind = to_upper(incoming.repeat_kind);

    if (last_display.empty() || last_display != incoming_display) {
        return false;
    }

    if (last_repeat_kind != incoming_repeat_kind) {
        return false;
    }

    if (incoming.repeat_count > 1) {
        return true;
    }
    return is_scroll_action(incoming.action);
}

bool HistoryBuffer::append_record(const HistoryRecord& rec) {
    if (limit_ == 0) {
        return false;
    }

    if (!records_.empty()) {
        HistoryRecord& last = records_.back();
        const bool incoming_repeat = rec.repeat_count > 1;
        const bool last_is_singleton = (last.repeat_count == 0 || last.repeat_count == 1)
            && to_upper(last.repeat_kind).empty();
        if (incoming_repeat
            && last_is_singleton
            && display_identity(last) == display_identity(rec)) {
            last = rec;
            return true;
        }

        if (should_merge(last, rec)) {
            if (last.key != rec.key || last.action != rec.action || last.combo != rec.combo) {
                last.repeat_count += rec.repeat_count;
            } else {
                last.repeat_count = std::max(last.repeat_count, rec.repeat_count);
            }
            return true;
        }
    }

    records_.push_back(rec);
    while (records_.size() > limit_) {
        records_.pop_front();
    }
    return true;
}

bool HistoryBuffer::flush_pending_record() {
    if (!pending_singleton_.has_value()) {
        return false;
    }

    const HistoryRecord rec = *pending_singleton_;
    pending_singleton_.reset();
    return append_record(rec);
}

std::string HistoryBuffer::format_line(const HistoryRecord& r) {
    std::string display = r.key;
    if (!r.combo.empty() && to_upper(r.combo) != "NONE") {
        display = r.combo;
    }
    display = readable_key_sequence(display);
    const unsigned int rc = r.repeat_count == 0 ? 1 : r.repeat_count;
    const std::string repeat_kind = to_upper(r.repeat_kind);
    if (rc > 1 && repeat_kind == "HOLD") {
        return display + " hold x" + std::to_string(rc);
    }
    if (rc > 1 && repeat_kind == "TAP") {
        return display + " tap x" + std::to_string(rc);
    }
    return display + " x" + std::to_string(rc);
}

std::string HistoryBuffer::readable_key_sequence(std::string raw) {
    raw = trim(raw);
    if (raw.empty()) {
        return "";
    }
    const std::size_t colon = raw.find(':');
    if (colon != std::string::npos) {
        raw = raw.substr(0, colon);
    }

    std::vector<std::string> parts;
    std::string tok;
    for (const char c : raw) {
        if (c == '+') {
            const std::string mapped = readable_token(trim(tok));
            if (!mapped.empty()) { parts.push_back(mapped); }
            tok.clear();
        } else {
            tok.push_back(c);
        }
    }
    const std::string mapped_last = readable_token(trim(tok));
    if (!mapped_last.empty()) { parts.push_back(mapped_last); }

    if (parts.empty()) { return ""; }
    std::string out = parts[0];
    for (std::size_t i = 1; i < parts.size(); ++i) {
        out += " ";
        out += parts[i];
    }
    return out;
}

std::string HistoryBuffer::readable_token(const std::string& token) {
    if (token.empty()) { return ""; }
    const std::string up = to_upper(token);
    const auto& t = readable_table();
    const auto  it = t.find(up);
    if (it != t.end()) { return it->second; }
    if (up.size() == 1) { return up; }
    if (up.size() >= 2 && up[0] == 'F') {
        bool all_digits = true;
        for (std::size_t i = 1; i < up.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(up[i])) == 0) {
                all_digits = false; break;
            }
        }
        if (all_digits) { return up; }
    }
    return title_case(up);
}

} // namespace Keymera::layers::input_overlay_window::model
