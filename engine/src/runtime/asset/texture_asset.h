#pragma once

#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"

namespace ptgn {

class Texture : public impl::Asset {
public:
	using Base = impl::Asset;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

	operator impl::TextureId() const;
};

} // namespace ptgn