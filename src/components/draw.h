#pragma once

#include <array>

#include "components/generic.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

namespace impl {

class RenderData;

void DrawTexture(impl::RenderData& ctx, const Entity& entity, bool flip_texture);

// @return Unscaled size of the entire texture in pixels.
[[nodiscard]] V2_int GetTextureSize(const Entity& entity);

// @return Unscaled size of the cropped texture in pixels.
[[nodiscard]] V2_int GetCroppedSize(const Entity& entity);

// @return Scaled size of the cropped texture in pixels.
[[nodiscard]] V2_float GetDisplaySize(const Entity& entity);

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const Entity& entity, bool flip_vertically
);

} // namespace impl

struct LineWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	LineWidth() : ArithmeticComponent{ 1.0f } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(LineWidth, value_)
};

struct TextureCrop {
	// Top left position (in pixels) within the texture from which the crop starts.
	V2_float position;

	// Size of the crop in pixels. Zero size will use full size of texture.
	V2_float size;

	friend bool operator==(const TextureCrop& a, const TextureCrop& b) {
		return a.position == b.position && a.size == b.size;
	}

	friend bool operator!=(const TextureCrop& a, const TextureCrop& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TextureCrop, position, size)
};

} // namespace ptgn