#pragma once

#include "ecs/ecs.h"

namespace ptgn {

class AssetManager;

namespace impl {

struct FontSize {
	float point_size{ 0.0f };
};

} // namespace impl

class Font {
public:
private:
	friend class AssetManager;

	ecs::Entity entity_;
};

} // namespace ptgn