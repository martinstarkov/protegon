#pragma once

#include <cstdint>

#include "components/generic.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;

struct Enabled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Enabled() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(Enabled, value_)
};

struct Visible : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Visible() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(Visible, value_)
};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(Depth, value_)
};

struct EntityDepthCompare {
	bool operator()(const Entity& a, const Entity& b) const;
};

} // namespace ptgn