#pragma once

#include "math/Vector2.h"

namespace engine {

struct SpriteComponent {
	SpriteComponent() = delete;
	SpriteComponent(const char* texture_path,
					V2_double size,
					V2_double scale = { 1.0, 1.0 }) :
		texture_path{ texture_path },
		size{ size },
		scale{ scale } {
	}
	~SpriteComponent() = default;
	const char* texture_path{ nullptr };
	V2_double scale;
	V2_double size;
};

} // namespace engine