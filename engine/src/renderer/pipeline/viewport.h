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

	V2_float GetCenter() const {
		return position + size * 0.5f;
	}

	bool operator==(const Viewport&) const = default;

	Viewport Resolve(ViewportSpace space, V2_int game_size, V2_int target_size) const {
		switch (space) {
			using enum ViewportSpace;

			case Game: {
				PTGN_ASSERT(game_size.IsPositive());
				PTGN_ASSERT(target_size.IsPositive());

				V2_float scale{ V2_float{ target_size } / game_size };

				return {
					.position{ FastCeil(position * scale) },
					.size{ FastCeil(size * scale) },
				};
			}

			case TargetPixels: return *this;

			case Normalized:
				return {
					.position{ position * target_size },
					.size{ size * target_size },
				};

			default: PTGN_ERROR("Unsupported viewport space");
		}
	}

	PTGN_SERIALIZE(Viewport, position, size)
};

} // namespace ptgn