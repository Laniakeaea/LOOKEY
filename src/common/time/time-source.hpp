#pragma once

#include <cstdint>

namespace lookey::common::time {

/// Returns the number of milliseconds since the Unix epoch.
/// Single implementation point; replace here for tests or alternative clocks.
unsigned long long now_epoch_ms();

} // namespace lookey::common::time
