#pragma once

#include <ecs/ecs.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <string>

#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

struct TextureBinding {
	std::uint32_t slot{ 0 };
	std::string uniform{ "u_Texture" };
};

namespace impl {

template <bool kFlipY>
constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	if constexpr (kFlipY) {
		return { V2_float{ 0.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, 0.0f },
				 V2_float{ 0.0f, 0.0f } };

	} else {
		return {
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		};
	}
}

/// @brief Values from [-1, 1]
constexpr std::array<V2_float, 4> GetNDCTextureCoordinates() {
	return { V2_float{ -1.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, -1.0f },
			 V2_float{ -1.0f, -1.0f } };
}

constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates(bool flip_y) {
	if (flip_y) {
		return GetDefaultTextureCoordinates<true>();
	} else {
		return GetDefaultTextureCoordinates<false>();
	}
}

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool flip_vertically,
	bool offset_texels
);

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, V2_float scale);
void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, Flip flip);

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