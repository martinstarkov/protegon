#pragma once

#include "components/generic.h"
#include "math/vector2.h"
#include "serialization/fwd.h"

namespace ptgn {

struct Transform {
	Transform() = default;

	Transform(
		const V2_float& position, float rotation = 0.0f, const V2_float& scale = { 1.0f, 1.0f }
	) :
		position{ position }, rotation{ rotation }, scale{ scale } {}

	[[nodiscard]] Transform RelativeTo(Transform parent) const {
		parent.position += position;
		parent.rotation += rotation;
		parent.scale	*= scale;
		return parent;
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

	[[nodiscard]] V2_float GetTotal() const;

	V2_float shake;
	V2_float bounce;
};

} // namespace impl

} // namespace ptgn