#pragma once

#include "core/math/vector2.h"
#include "ecs/ecs.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class AssetManager;

class Texture {
public:
	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	friend class AssetManager;

	ecs::Entity entity_;
};

} // namespace ptgn