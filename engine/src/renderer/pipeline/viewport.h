#pragma once

#include <optional>

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

	constexpr V2_float GetCenter() const {
		return position + size * 0.5f;
	}

	constexpr bool operator==(const Viewport&) const = default;

	PTGN_REFLECT(Viewport, position, size)
};

constexpr Viewport GetLogicalViewport(
	std::optional<Viewport> raw_viewport, ViewportSpace viewport_space, V2_int logical_size
) {
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size must be positive");

	if (!raw_viewport.has_value()) {
		return { .position{}, .size{ logical_size } };
	}

	if (viewport_space == ViewportSpace::Logical) {
		return raw_viewport.value();
	} else if (viewport_space == ViewportSpace::Normalized) {
		return { raw_viewport.value().position * logical_size,
				 raw_viewport.value().size * logical_size };
	} else {
		PTGN_ERROR("Unsupported viewport space");
	}
}

constexpr Viewport GetDisplayViewport(
	std::optional<Viewport> raw_viewport, ViewportSpace viewport_space, V2_int logical_size,
	V2_int target_size, bool scene_target
) {
	PTGN_ASSERT(target_size.IsPositive(), "Display target size must be positive");
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size must be positive");

	if (raw_viewport.has_value()) {
		if (viewport_space == ViewportSpace::Logical) {
			if (scene_target) {
				V2_float scale{ V2_float{ target_size } / logical_size };

				return {
					.position{ raw_viewport.value().position * scale },
					.size{ Max(raw_viewport.value().size * scale, { 1, 1 }) },
				};
			} else {
				return raw_viewport.value();
			}
		} else if (viewport_space == ViewportSpace::Normalized) {
			PTGN_ASSERT(
				WithinRangeInclusive(raw_viewport.value().position, 0.0f, 1.0f),
				"Position must be in [0, 1]"
			);
			PTGN_ASSERT(
				WithinRangeInclusive(raw_viewport.value().size, 0.0f, 1.0f),
				"Size must be in [0, 1]"
			);
			return {
				.position{ raw_viewport.value().position * target_size },
				.size{ raw_viewport.value().size * target_size },
			};
		}
	}

	return { .position{}, .size{ target_size } };
}

} // namespace ptgn