#pragma once

#include <ecs/ecs.h>

#include <ostream>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

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

	friend std::ostream& operator<<(std::ostream& os, const Texture& t) {
		os << "{ texture id: " << t.operator impl::TextureId();
		os << ", size: " << t.GetSize() << " }";
		return os;
	}

	// TODO: Consider moving this to private and not exposing any render functions that use ids.
	operator impl::TextureId() const; // NOSONAR
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Texture> {
	std::size_t operator()(const ptgn::Texture& texture) const {
		return ptgn::Hash(texture.GetEntity());
	}
};