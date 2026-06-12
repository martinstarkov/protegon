#pragma once

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class ViewportSpace {
	Logical,
	Normalized,
};

struct Viewport {
	/// @brief Top left position in pixels relative to its display target.
	V2_float position;

	/// @brief Size in pixels.
	V2_float size;

	V2_float GetCenter() const {
		return position + size * 0.5f;
	}

	bool operator==(const Viewport&) const = default;

	PTGN_SERIALIZE(Viewport, position, size)
};

constexpr Viewport GetLogicalViewport(
	Viewport viewport, ViewportSpace viewport_space, V2_int logical_size
) {
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size must be positive");

	if (viewport_space == ViewportSpace::Logical) {
		return viewport;
	} else if (viewport_space == ViewportSpace::Normalized) {
		return { viewport.position * logical_size, viewport.size * logical_size };
	} else {
		PTGN_ERROR("Unsupported viewport space");
	}
}

constexpr Viewport GetDisplayViewport(
	Viewport viewport, ViewportSpace viewport_space, V2_int logical_size, V2_int display_target_size
) {
	PTGN_ASSERT(display_target_size.IsPositive(), "Display target size must be positive");
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size must be positive");

	if (viewport_space == ViewportSpace::Logical) {
		V2_float scale{ V2_float{ display_target_size } / logical_size };

		return {
			.position{ viewport.position * scale },
			.size{ Max(viewport.size * scale, { 1, 1 }) },
		};
	} else if (viewport_space == ViewportSpace::Normalized) {
		return {
			.position{ viewport.position * display_target_size },
			.size{ viewport.size * display_target_size },
		};
	} else {
		PTGN_ERROR("Unsupported viewport space");
	}
}

} // namespace ptgn