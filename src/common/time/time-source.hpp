#pragma once

#include <cstdint>

namespace Keymera::common::time {

/// Returns the number of milliseconds since the Unix epoch.
/// Single implementation point; replace here for tests or alternative clocks.
unsigned long long now_epoch_ms();

} // namespace Keymera::common::time
