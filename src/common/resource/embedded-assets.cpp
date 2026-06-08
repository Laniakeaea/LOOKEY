#include "embedded-assets.hpp"

namespace lookey::common::resource {

std::optional<EmbeddedAssetView> find_embedded_asset(std::string_view path) {
    (void)path;
    return std::nullopt;
}

} // namespace lookey::common::resource