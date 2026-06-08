#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace Keymera::common::resource {

struct EmbeddedAssetView {
    const unsigned char* data{nullptr};
    std::size_t size{0};
};

std::optional<EmbeddedAssetView> find_embedded_asset(std::string_view path);

} // namespace Keymera::common::resource