#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "renderer/resources/id.h"

namespace ptgn {

namespace impl {

using Index = std::uint32_t;

class Batch {
public:
private:
	std::vector<std::byte> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;
};

} // namespace impl

} // namespace ptgn
