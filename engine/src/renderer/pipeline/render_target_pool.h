#pragma once

#include <cstdint>
#include <limits>

#include "core/math/vector2.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

struct PooledTarget {
	RenderTargetObject target;
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

struct ResourceLifetime {
	bool used{ false };		   // was this resource used by any node?
	std::size_t first_use{ std::numeric_limits<std::size_t>::max(
	) };					   // first node index where it appears
	std::size_t last_use{ 0 }; // last node index where it appears
};

struct PhysicalTransient {
	RenderTargetId target;
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
};

struct LiveTransient {
	RenderResourceId resource{ 0 };
	PhysicalTransient physical;
	std::size_t last_use{ 0 };
};

} // namespace impl

} // namespace ptgn