#pragma once

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class ViewportSpace {
	Game,
	TargetPixels,
	Normalized,
};

struct Viewport {
	/// @brief Position of the top left of the viewport relative to the display render target.
	V2_int position;

	/// @brief Size of the viewport in pixels.
	V2_int size;

	bool operator==(const Viewport&) const = default;

	PTGN_SERIALIZE(Viewport, position, size)
};

constexpr Viewport GetLogicalViewport(
	Viewport raw_viewport, ViewportSpace viewport_space, V2_int game_size
) {
	if (viewport_space != ViewportSpace::Normalized) {
		return raw_viewport;
	}

	return { raw_viewport.position * game_size, raw_viewport.size * game_size };
}

constexpr Viewport GetRenderViewport(
	Viewport raw_viewport, ViewportSpace viewport_space, V2_int game_size, V2_int target_size
) {
	switch (viewport_space) {
		using enum ViewportSpace;

		case Game: {
			PTGN_ASSERT(game_size.IsPositive());
			PTGN_ASSERT(target_size.IsPositive());

			V2_float scale{ V2_float{ target_size } / game_size };

			return {
				.position{ FastCeil(raw_viewport.position * scale) },
				.size{ FastCeil(raw_viewport.size * scale) },
			};
		}

		case TargetPixels: return raw_viewport;

		case Normalized:
			return {
				.position{ raw_viewport.position * target_size },
				.size{ raw_viewport.size * target_size },
			};

		default: PTGN_ERROR("Unsupported viewport space");
	}
}

} // namespace ptgn