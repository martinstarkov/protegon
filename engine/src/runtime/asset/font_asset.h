#pragma once

#include "runtime/ecs/entity.h"

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

	Entity entity_;
};

} // namespace ptgn