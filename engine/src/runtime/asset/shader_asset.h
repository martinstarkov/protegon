#pragma once

#include "ecs/ecs.h"

namespace ptgn {

class AssetManager;

class Shader {
public:
private:
	friend class AssetManager;

	ecs::Entity entity_;
};

} // namespace ptgn