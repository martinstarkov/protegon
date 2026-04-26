#pragma once

#include <string>
#include <string_view>

#include "serialization/serialize.h"

namespace ptgn::impl {

constexpr std::string_view kDefaultTag{ "Unnamed Entity" };

struct Tag {
	Tag() = default;

	Tag(std::string_view tag) : value{ tag } {} // NOSONAR

	operator std::string_view() const {			// NOSONAR
		return value;
	}

	std::string value{ kDefaultTag };

	PTGN_SERIALIZE_VALUE(Tag, value)
};

} // namespace ptgn::impl