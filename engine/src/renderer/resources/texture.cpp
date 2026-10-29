#include "renderer/resources/texture.h"

#include <ecs/ecs.h>

#include <array>
#include <utility>

#include "core/assert.h"
#include "core/graphics/flip.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool flip_vertically,
	bool offset_texels
) {
	if (!texture_size.IsPositive()) {
		PTGN_WARN("Texture size ", texture_size, " must be positive, using default texture coordinates");
		return GetDefaultTextureCoordinates(flip_vertically);
	}

	if (source_position.x >= texture_size.x || source_position.y >= texture_size.y) {
		PTGN_WARN("Source position ", source_position, " out of texture size ", texture_size, " bounds, using default texture coordinates");
		return GetDefaultTextureCoordinates(flip_vertically);
	}

	if (source_size.IsZero()) {
		source_size = texture_size - source_position;
	}

	auto texel{ offset_texels ? (0.5f / texture_size) : V2_float{ 0, 0 } };

	auto min{ (source_position - texel) / texture_size };
	auto max{ (source_position + source_size - texel) / texture_size };

	if (max.x > 1.0f || max.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size: ", max);
	}

	std::array uv{ min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };

	if (flip_vertically) {
		FlipTextureCoordinates(uv, Flip::Vertical);
	}

	return uv;
}

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, V2_float scale) {
	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	using enum Flip;

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Vertical);
	}
}

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, Flip flip) {
	auto flip_x = [&]() {
		std::swap(tex_coords[0].x, tex_coords[1].x);
		std::swap(tex_coords[2].x, tex_coords[3].x);
	};
	auto flip_y = [&]() {
		std::swap(tex_coords[0].y, tex_coords[3].y);
		std::swap(tex_coords[1].y, tex_coords[2].y);
	};
	switch (flip) {
		using enum Flip;
		case None:		 break;
		case Vertical:	 flip_y(); break;
		case Horizontal: flip_x(); break;
		case Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unknown Flip: ", std::to_underlying(flip));
	}
}

V2_int TextureObject::GetSize() const {
	return GetDesc().size;
}

TextureFormat TextureObject::GetFormat() const {
	return GetDesc().format;
}

TextureParams TextureObject::GetParams() const {
	return GetDesc().params;
}

TextureDesc TextureObject::GetDesc() const {
	PTGN_ASSERT(*this, "Texture object must be valid");
	return renderer->GetDesc(*this).value();
}

} // namespace impl

V2_int Texture::GetSize() const {
	return GetEntity().Get<impl::TextureObject>().GetSize();
}

TextureFormat Texture::GetFormat() const {
	return GetEntity().Get<impl::TextureObject>().GetFormat();
}

TextureParams Texture::GetParams() const {
	return GetEntity().Get<impl::TextureObject>().GetParams();
}

TextureDesc Texture::GetDesc() const {
	return GetEntity().Get<impl::TextureObject>().GetDesc();
}

Texture::operator impl::TextureId() const {
	return GetEntity().Get<impl::TextureObject>();
}

} // namespace ptgn