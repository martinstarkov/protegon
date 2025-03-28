#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>

#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

[[nodiscard]] json LoadJson(const path& filepath);

// template <typename T>
// void SetMember(T& value, std::string_view key, const T& default_value) {
//	if (j.contains(key)) {
//		j.at(key).get_to(value);
//	} else {
//		value = default_value;
//	}
// }

} // namespace ptgn
