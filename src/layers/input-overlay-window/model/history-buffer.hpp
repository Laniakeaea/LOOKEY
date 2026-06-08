#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace Keymera::layers::input_overlay_window::model {

struct HistoryRecord {
    std::string  key;
    std::string  action;
    std::string  combo;
    std::string  repeat_kind;
    unsigned int repeat_count{1};
};

/// All history insertion, dedup, trimming and export in one place.
class HistoryBuffer {
public:
    explicit HistoryBuffer() = default;

    void set_limit(unsigned int limit);
    unsigned int limit() const { return limit_; }

    /// Returns true when the record was accepted (limit > 0 and recordable action).
    bool push(const std::string& key,
              const std::string& action,
              const std::string& combo,
              const std::string& repeat_kind,
              unsigned int       repeat_count);
    bool push_state_snapshot(const std::string& key,
                             const std::string& combo);

    void clear();

    /// Returns newest-first formatted text lines (up to limit_).
    std::vector<std::string> export_text_lines() const;

    const std::deque<HistoryRecord>& records() const { return records_; }

private:
    static bool should_record(const std::string& key, const std::string& action);
    static bool is_scroll_action(const std::string& action);
    static std::string display_identity(const HistoryRecord& r);
    static bool should_merge(const HistoryRecord& last,
                             const HistoryRecord& incoming);
    static std::string format_line(const HistoryRecord& r);
    static std::string readable_key_sequence(std::string raw);
    static std::string readable_token(const std::string& token);
    bool append_record(const HistoryRecord& rec);
    bool flush_pending_record();

    std::deque<HistoryRecord> records_;
    std::optional<HistoryRecord> pending_singleton_;
    unsigned int              limit_{5};
};

} // namespace Keymera::layers::input_overlay_window::model
