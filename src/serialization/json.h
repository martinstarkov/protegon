#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>

#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

void SaveJson(const json& j, const path& filepath);

[[nodiscard]] json LoadJson(const path& filepath);

// template <typename T>
// void SetMember(T& value, std::string_view key, const T& default_value) {
//	if (j.contains(key)) {
//		j.at(key).get_to(value);
//	} else {
//		value = default_value;
//	}
// }

namespace tt {

template <typename T>
inline constexpr bool has_to_json_v = nlohmann::detail::has_to_json<json, T>::value;

template <typename T>
inline constexpr bool has_from_json_v = nlohmann::detail::has_from_json<json, T>::value;

template <typename T>
inline constexpr bool is_json_convertible_v = has_to_json_v<T> && has_from_json_v<T>;

} // namespace tt

} // namespace ptgn
