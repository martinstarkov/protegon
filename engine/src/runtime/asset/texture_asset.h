#pragma once

#include <cstdint>

#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Texture : public impl::RefCountedAsset<Texture> {
public:
	using RefCountedAsset::RefCountedAsset;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	friend class impl::RefCountedAsset<Texture>;

	void Destroy();
};

} // namespace ptgn