#pragma once

#include <string>
#include <string_view>

#include "serialization/serialize.h"

namespace ptgn::impl {

inline constexpr std::string_view kDefaultTag{ "Entity" };

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

#define PTGN_DEFAULT_NAME(entity, tag) entity.SetTag(tag)