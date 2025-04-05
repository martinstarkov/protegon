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

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	PTGN_SERIALIZER_REGISTER_NAMELESS(Depth, value_)
};

} // namespace ptgn