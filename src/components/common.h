#pragma once

#include <cstdint>

#include "components/generic.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Enabled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Enabled() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS(Enabled, value_)
};

struct Visible : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Visible() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS(Visible, value_)
};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;

	PTGN_SERIALIZER_REGISTER_NAMELESS(Depth, value_)
};

} // namespace ptgn