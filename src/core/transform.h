#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Transform {
	Transform() = default;

	explicit Transform(const V2_float& position) : position{ position } {}

	Transform(const V2_float& position, float rotation, const V2_float& scale = { 1.0f, 1.0f }) :
		position{ position }, rotation{ rotation }, scale{ scale } {}

	[[nodiscard]] Transform RelativeTo(Transform parent) const {
		parent.position += position;
		parent.rotation += rotation;
		parent.scale	*= scale;
		return parent;
	}

	friend bool operator==(const Transform& a, const Transform& b) {
		return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
	}

	friend bool operator!=(const Transform& a, const Transform& b) {
		return !(a == b);
	}

	V2_float position;
	float rotation{ 0.0f };
	V2_float scale{ 1.0f, 1.0f };
};

} // namespace ptgn