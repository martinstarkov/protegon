#pragma once

#include <cstdint>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class RenderTarget {
public:
	void Resize(V2_int new_size);
	void Bind();
	void Clear(Color color = color::Transparent);

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	Entity entity_;
};

} // namespace ptgn