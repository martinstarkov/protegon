#pragma once

#include <nlohmann/json.hpp>
#include <string_view>
#include <variant>

#include "core/util/file.h"
#include "core/util/type_info.h"
#include "serialization/json/fwd.h"

namespace ptgn {

void SaveJson(const json& j, const path& filepath, bool indent = true);

// Note: Do not brace initialize JSON objects.
// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
[[nodiscard]] json LoadJson(const path& filepath);

// template <typename T>
// void SetMember(T& value, std::string_view key, const T& default_value) {
//	if (j.contains(key)) {
//		j.at(key).get_to(value);
//	} else {
//		value = default_value;
//	}
// }

template <typename T>
concept JsonSerializable = nlohmann::detail::has_to_json<json, T>::value;

template <typename T>
concept JsonDeserializable = nlohmann::detail::has_from_json<json, T>::value;

template <typename T>
concept JsonConvertible = JsonSerializable<T> && JsonDeserializable<T>;

} // namespace ptgn
