#pragma once

#include <string_view>

#include "core/util/strong_string.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Tag : StrongString<Tag> {
	using StrongString::StrongString;

	constexpr Tag() : StrongString{ "Entity" } {}

	PTGN_REFLECT_VALUE(Tag, value)
};

} // namespace ptgn