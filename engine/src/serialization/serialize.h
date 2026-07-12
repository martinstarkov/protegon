#pragma once

#include <concepts>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/detail/abi_macros.hpp>
#include <nlohmann/detail/iterators/iter_impl.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/util/macro.h"
#include "core/util/reflection.h"
#include "serialization/json/json.h"

namespace ptgn::impl {

// Try to set the value of type T into the variant data if it fails, do nothing
template <typename T, typename... Ts>
void variant_from_json(const json& j, std::variant<Ts...>& data) {
	try {
		data = j.get<T>();
	} catch (...) {
		/* Ignore */
	}
}

} // namespace ptgn::impl

NLOHMANN_JSON_NAMESPACE_BEGIN

template <typename... Ts>
struct adl_serializer<std::variant<Ts...>> {
	static void to_json(json& j, const std::variant<Ts...>& data) {
		// Will call j = v automatically for the right type
		std::visit([&j](const auto& v) { j = v; }, data);
	}

	static void from_json(const json& j, std::variant<Ts...>& data) {
		// Call variant_from_json for all types, only one will succeed
		(::ptgn::impl::variant_from_json<Ts>(j, data), ...);
	}
};

NLOHMANN_JSON_NAMESPACE_END

namespace ptgn::impl {

template <class T>
void optional_to_json(json& j, std::string_view name, const std::optional<T>& value) {
	if (value) {
		j[name] = *value;
	}
}

template <class T>
void optional_from_json(const json& j, std::string_view name, std::optional<T>& value) {
	const auto it{ j.find(name) };
	if (it != j.end()) {
		value = it->get<T>();
	} else {
		value = std::nullopt;
	}
}

template <class T>
void optional_to_json(json& j, const std::optional<T>& value) {
	if (value) {
		j = *value;
	}
}

template <class T>
void optional_from_json(const json& j, std::optional<T>& value) {
	if (j.is_null()) {
		value = std::nullopt;
	} else {
		value = j.get<T>();
	}
}

template <typename>
inline constexpr bool is_optional{ false };

template <typename T>
inline constexpr bool is_optional<std::optional<T>>{ true };

template <typename T>
void extended_to_json(std::string_view key, json& j, const T& value) {
	if constexpr (is_optional<T>) {
		optional_to_json(j, key, value);
	} else if constexpr (JsonSerializable<T>) {
		j[key] = value;
	}
}

template <typename T>
void extended_from_json(std::string_view key, const json& j, T& value) {
	if constexpr (is_optional<T>) {
		optional_from_json(j, key, value);
	} else if constexpr (JsonDeserializable<T>) {
		j.at(key).get_to(value);
	}
}

template <typename T>
void extended_to_json(json& j, const T& value) {
	if constexpr (is_optional<T>) {
		optional_to_json(j, value);
	} else if constexpr (JsonSerializable<T>) {
		j = value;
	}
}

template <typename T>
void extended_from_json(const json& j, T& value) {
	if constexpr (is_optional<T>) {
		optional_from_json(j, value);
	} else if constexpr (JsonDeserializable<T>) {
		j.get_to(value);
	}
}

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType T>
[[nodiscard]] bool try_enum_from_json(const json& j, T& v) {
	if (!j.is_string()) {
		return false;
	}

	if (const auto parsed{ ::magic_enum::enum_cast<T>(j.get<std::string>()) }; parsed.has_value()) {
		v = parsed.value();
		return true;
	}

	return false;
}

template <EnumType T>
[[nodiscard]] bool try_enum_to_json(json& j, T v) {
	if (const auto name{ ::magic_enum::enum_name(v) }; !name.empty()) {
		j = name;
		return true;
	}
	return false;
}

template <EnumType T>
void enum_from_json(const json& j, T& v) {
	if (!try_enum_from_json<T>(j, v)) {
		throw std::invalid_argument(
			"Unknown enum value in json for enum deserialization: " + j.dump()
		);
	}
}

template <EnumType T>
void enum_to_json(json& j, const T v) {
	if (!try_enum_to_json<T>(j, v)) {
		throw std::invalid_argument("Unknown enum value: " + std::to_string(std::to_underlying(v)));
	}
}

constexpr std::string_view StripTrailingUnderscore(std::string_view name) {
	return (!name.empty() && name.back() == '_') ? name.substr(0, name.size() - 1) : name;
}

template <typename T>
void reflected_members_to_json(json& j, const T& value) {
	j = json::object();
	auto members{ ReflectMembers(value) };

	std::apply(
		[&]<typename... TMember>(const TMember&... member) {
			(extended_to_json(StripTrailingUnderscore(member.name), j, member.value), ...);
		},
		members
	);
}

template <typename T>
void reflected_members_from_json(const json& j, T& value) {
	auto members{ ReflectMembers(value) };

	std::apply(
		[&]<typename... TMember>(TMember&&... member) {
			(extended_from_json(StripTrailingUnderscore(member.name), j, member.value), ...);
		},
		members
	);
}

template <typename T>
void reflected_value_to_json(json& j, const T& value) {
	auto member{ ReflectValue(value) };
	extended_to_json(j, member.value);
}

template <typename T>
void reflected_value_from_json(const json& j, T& value) {
	auto member{ ReflectValue(value) };
	extended_from_json(j, member.value);
}

inline void reflected_empty_to_json(json& j, std::string_view expected_name) {
	j = expected_name;
}

inline void reflected_empty_from_json(const json& j, std::string_view expected_name) {
	const auto name{ j.get<std::string>() };
	if (name != expected_name) {
		throw std::runtime_error(
			"Invalid struct name found in JSON: " + name +
			", expected: " + std::string{ expected_name }
		);
	}
}

} // namespace ptgn::impl

#define PTGN_IMPL_REFLECTED_JSON_MEMBERS(Type)                                                     \
	template <typename PTGNReflectedType>                                                          \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                        \
	friend void to_json(::ptgn::json& nlohmann_json_j, const PTGNReflectedType& nlohmann_json_t) { \
		::ptgn::impl::reflected_members_to_json(nlohmann_json_j, nlohmann_json_t);                 \
	}                                                                                              \
	template <typename PTGNReflectedType>                                                          \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                        \
	friend void from_json(                                                                         \
		const ::ptgn::json& nlohmann_json_j, PTGNReflectedType& nlohmann_json_t                    \
	) {                                                                                            \
		::ptgn::impl::reflected_members_from_json(nlohmann_json_j, nlohmann_json_t);               \
	}

#define PTGN_IMPL_REFLECTED_JSON_VALUE(Type)                                                       \
	template <typename PTGNReflectedType>                                                          \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                        \
	friend void to_json(::ptgn::json& nlohmann_json_j, const PTGNReflectedType& nlohmann_json_t) { \
		::ptgn::impl::reflected_value_to_json(nlohmann_json_j, nlohmann_json_t);                   \
	}                                                                                              \
	template <typename PTGNReflectedType>                                                          \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                        \
	friend void from_json(                                                                         \
		const ::ptgn::json& nlohmann_json_j, PTGNReflectedType& nlohmann_json_t                    \
	) {                                                                                            \
		::ptgn::impl::reflected_value_from_json(nlohmann_json_j, nlohmann_json_t);                 \
	}

#define PTGN_IMPL_REFLECTED_JSON_EMPTY(Type)                                                 \
	template <typename PTGNReflectedType>                                                    \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                  \
	friend void to_json(::ptgn::json& nlohmann_json_j, const PTGNReflectedType&) {           \
		::ptgn::impl::reflected_empty_to_json(nlohmann_json_j, std::string_view{ #Type });   \
	}                                                                                        \
	template <typename PTGNReflectedType>                                                    \
		requires std::same_as<std::remove_cvref_t<PTGNReflectedType>, Type>                  \
	friend void from_json(const ::ptgn::json& nlohmann_json_j, PTGNReflectedType&) {         \
		::ptgn::impl::reflected_empty_from_json(nlohmann_json_j, std::string_view{ #Type }); \
	}

/// @brief Use this OUTSIDE the enum declaration.
#define PTGN_REFLECT_ENUM(Type)                                 \
	inline void to_json(::ptgn::json& j, Type value) {          \
		::ptgn::impl::enum_to_json(j, value);                   \
	}                                                           \
	inline void from_json(const ::ptgn::json& j, Type& value) { \
		::ptgn::impl::enum_from_json(j, value);                 \
	}

/// @brief Use this INSIDE a class/struct. The listed members are editable reflection members and
/// receive fallback JSON serialization. A user-provided non-template to_json and/or from_json
/// overload is preferred over the generated fallback template, so custom serialization does not
/// conflict with this macro.
#define PTGN_REFLECT(Type, ...)                  \
	PTGN_IMPL_REFLECT_MEMBERS(Type, __VA_ARGS__) \
	PTGN_IMPL_REFLECTED_JSON_MEMBERS(Type)

/// @brief Reflects and serializes the type directly as one member value.
#define PTGN_REFLECT_VALUE(Type, member)  \
	PTGN_IMPL_REFLECT_VALUE(Type, member) \
	PTGN_IMPL_REFLECTED_JSON_VALUE(Type)

/// @brief Reflects an empty type and serializes it as its type name.
#define PTGN_REFLECT_EMPTY(Type)  \
	PTGN_IMPL_REFLECT_EMPTY(Type) \
	PTGN_IMPL_REFLECTED_JSON_EMPTY(Type)

/// @brief Reflects a base class followed by this type's listed members and provides fallback JSON.
#define PTGN_REFLECT_DERIVED(Type, Base, ...)                  \
	PTGN_IMPL_REFLECT_DERIVED_MEMBERS(Type, Base, __VA_ARGS__) \
	PTGN_IMPL_REFLECTED_JSON_MEMBERS(Type)
