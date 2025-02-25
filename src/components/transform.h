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

	V2_float position;
	float rotation{ 0.0f };
	V2_float scale{ 1.0f, 1.0f };
};

void to_json(json& j, const Transform& t);

void from_json(const json& j, Transform& t);

struct RotationCenter : public Vector2Component<float> {
	using Vector2Component::Vector2Component;

	RotationCenter() : Vector2Component{ V2_float{ 0.5f, 0.5f } } {}
};

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;
};

} // namespace ptgn