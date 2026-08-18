#pragma once

#include <string>
#include <vector>

#include "serialization/serialize.h"

namespace ptgn {

struct Group {
	std::vector<std::string> groups;

	PTGN_REFLECT_VALUE(Group, groups)
};

} // namespace ptgn
