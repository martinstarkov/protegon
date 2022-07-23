#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct SpriteComponent {
	SpriteComponent() = delete;
	~SpriteComponent() = default;
	SpriteComponent(const char* texture_path,
					V2_double size,
					V2_double scale = { 1.0, 1.0 }) :
		texture_path{ texture_path },
		size{ size },
		scale{ scale } {
	}
	const char* texture_path{ nullptr };
	V2_double size;
	V2_double scale;
};

} // namespace ptgn