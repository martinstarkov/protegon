#pragma once

#include <cstdint>

#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Texture {
public:
	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	Entity entity_;
};

} // namespace ptgn