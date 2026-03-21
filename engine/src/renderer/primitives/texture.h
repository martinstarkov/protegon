#pragma once

#include <ostream>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"

namespace ptgn {

namespace impl {

class TextureObject : public Resource<TextureId> {
public:
	using Base = Resource<TextureId>;
	using Base::Base;

	V2_int GetSize() const;

	TextureFormat GetFormat() const;
};

} // namespace impl

class Texture : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

	operator impl::TextureId() const;
};

std::ostream& operator<<(std::ostream& o, const Texture& t);

} // namespace ptgn