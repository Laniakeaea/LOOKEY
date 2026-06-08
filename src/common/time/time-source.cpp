#include "time-source.hpp"

#include <chrono>

namespace Keymera::common::time {

unsigned long long now_epoch_ms() {
    const auto now = std::chrono::system_clock::now();
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<unsigned long long>(ms.count());
}

} // namespace Keymera::common::time
