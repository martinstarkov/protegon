#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"

namespace ptgn {

namespace impl {

using Index = std::uint32_t;

struct Batch {
	PipelineId pipeline{ 0 };
	RenderState state;
	MaterialState material;

	std::vector<std::byte> vertices;
	std::vector<Index> indices;

	// Dynamic texture-array bindings accumulated from PrimitiveRange::texture.
	std::vector<ResolvedTextureBinding> texture_bindings;
};

} // namespace impl

} // namespace ptgn
