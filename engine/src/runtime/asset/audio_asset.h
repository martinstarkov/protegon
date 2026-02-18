#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn {

class AssetManager;

class Audio {
public:
private:
	friend class AssetManager;

	Entity entity_;
};

} // namespace ptgn