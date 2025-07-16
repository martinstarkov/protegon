#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>
#include <variant>

#include "common/type_info.h"
#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

void SaveJson(const json& j, const path& filepath, bool indent = true);

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

NLOHMANN_JSON_NAMESPACE_BEGIN

namespace impl {

template <typename T, typename... Ts>
bool variant_from_json(const nlohmann::json& j, std::variant<Ts...>& data) {
	if (j.at("type").get<std::string_view>() != ptgn::type_name_without_namespaces<T>()) {
		return false;
	}
	data = j.at("data").get<T>();
	return true;
}

} // namespace impl

template <typename... Ts>
struct adl_serializer<std::variant<Ts...>> {
	static void to_json(json& j, const std::variant<Ts...>& data) {
		std::visit(
			[&j](const auto& v) {
				using T	  = std::decay_t<decltype(v)>;
				j["type"] = ptgn::type_name_without_namespaces<T>();
				j["data"] = v;
			},
			data
		);
	}

	static void from_json(const json& j, std::variant<Ts...>& data) {
		// Call variant_from_json for all types, only one will succeed
		bool found = (impl::variant_from_json<Ts>(j, data) || ...);
		if (!found) {
			throw std::bad_variant_access();
		}
	}
};

NLOHMANN_JSON_NAMESPACE_END
