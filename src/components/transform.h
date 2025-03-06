#pragma once

#include "components/generic.h"
#include "math/vector2.h"
#include "serialization/fwd.h"

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

void to_json(json& j, const Transform& t);

void from_json(const json& j, Transform& t);

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;
};

namespace impl {

// Various transform offsets which do not permanently change the transform of an entity, i.e. camera
// shake, bounce.
struct Offsets {
	Offsets() = default;

	[[nodiscard]] Transform GetTotal() const;

	Transform shake;
	Transform bounce;
};

} // namespace impl

} // namespace ptgn