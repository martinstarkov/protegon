#pragma once

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
#include <type_traits>
#include <utility>
#include <variant>

#include "core/util/macro.h"
#include "core/util/macro_loop.h"
#include "serialization/json/json.h"

namespace ptgn::impl {

// Try to set the value of type T into the variant data if it fails, do nothing
template <typename T, typename... Ts>
void variant_from_json(const json& j, std::variant<Ts...>& data) {
	try {
		data = j.get<T>();
	} catch (...) { /* Ignore */
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
	const auto it = j.find(name);
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
constexpr bool is_optional = false;

template <typename T>
constexpr bool is_optional<std::optional<T>> = true;

template <typename T>
void extended_to_json(std::string_view key, json& j, const T& value) {
	if constexpr (is_optional<T>) {
		optional_to_json(j, key, value);
	} else {
		j[key] = value;
	}
}

template <typename T>
void extended_from_json(std::string_view key, const json& j, T& value) {
	if constexpr (is_optional<T>) {
		optional_from_json(j, key, value);
	} else {
		j.at(key).get_to(value);
	}
}

template <typename T>
void extended_to_json(json& j, const T& value) {
	if constexpr (is_optional<T>) {
		optional_to_json(j, value);
	} else {
		j = value;
	}
}

template <typename T>
void extended_from_json(const json& j, T& value) {
	if constexpr (is_optional<T>) {
		optional_from_json(j, value);
	} else {
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
		v = *parsed;
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

} // namespace ptgn::impl

#define PTGN_IMPL_EXTEND_JSON_TO(v1)                                                    \
	::ptgn::impl::extended_to_json(                                                     \
		::ptgn::impl::StripTrailingUnderscore(#v1), nlohmann_json_j, nlohmann_json_t.v1 \
	);
#define PTGN_IMPL_EXTEND_JSON_FROM(v1)                                                  \
	::ptgn::impl::extended_from_json(                                                   \
		::ptgn::impl::StripTrailingUnderscore(#v1), nlohmann_json_j, nlohmann_json_t.v1 \
	);

#define PTGN_IMPL_EXTEND_JSON_TO_VALUE(v1) \
	::ptgn::impl::extended_to_json(nlohmann_json_j, nlohmann_json_t.v1);
#define PTGN_IMPL_EXTEND_JSON_FROM_VALUE(v1) \
	::ptgn::impl::extended_from_json(nlohmann_json_j, nlohmann_json_t.v1);

#define PTGN_IMPL_SERIALIZE_ENUM_FROM_JSON_CASE(EnumCase, Type) \
	if (s == PTGN_STRINGIFY(EnumCase)) {                        \
		value = Type::EnumCase;                                 \
		return;                                                 \
	}

#define PTGN_IMPL_SERIALIZE_ENUM_TO_JSON_CASE(EnumCase, Type) \
	case Type::EnumCase: j = PTGN_STRINGIFY(EnumCase); return;

/// @brief Use this OUTSIDE the enum declaration.
#define PTGN_SERIALIZE_ENUM(Type)                               \
	inline void to_json(::ptgn::json& j, Type value) {          \
		::ptgn::impl::enum_to_json(j, value);                   \
	}                                                           \
	inline void from_json(const ::ptgn::json& j, Type& value) { \
		::ptgn::impl::enum_from_json(j, value);                 \
	}

/// @brief Use this OUTSIDE the enum declaration.
/// Declares JSON serialization for an enum using an explicit list of enum cases.
#define PTGN_SERIALIZE_ENUM_MANUAL(Type, ...)                                                 \
	inline void to_json(::ptgn::json& j, Type value) {                                        \
		static_assert(std::is_enum_v<Type>);                                                  \
		switch (value) {                                                                      \
			PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_ENUM_TO_JSON_CASE, Type, __VA_ARGS__)           \
			default:                                                                          \
				throw std::runtime_error(                                                     \
					"Unknown " PTGN_STRINGIFY(Type) " enum value: " +                         \
					std::to_string(std::to_underlying(value))                                 \
				);                                                                            \
		}                                                                                     \
	}                                                                                         \
	inline void from_json(const ::ptgn::json& j, Type& value) {                               \
		static_assert(std::is_enum_v<Type>);                                                  \
		if (j.is_string()) {                                                                  \
			const auto s{ j.get<std::string>() };                                             \
			PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_ENUM_FROM_JSON_CASE, Type, __VA_ARGS__)         \
			throw std::runtime_error("Invalid enum name for " PTGN_STRINGIFY(Type) ": " + s); \
		}                                                                                     \
		value = static_cast<Type>(j.get<std::underlying_type_t<Type>>());                     \
	}

/// @brief Use this INSIDE the class/struct body.
#define PTGN_SERIALIZE(Type, ...)                                                              \
	friend inline void to_json(::ptgn::json& nlohmann_json_j, const Type& nlohmann_json_t) {   \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_TO, __VA_ARGS__))       \
	}                                                                                          \
	friend inline void from_json(const ::ptgn::json& nlohmann_json_j, Type& nlohmann_json_t) { \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_FROM, __VA_ARGS__))     \
	}

/// @brief Use this INSIDE the class/struct body.
/// Serializes directly as that value, without a field name.
#define PTGN_SERIALIZE_VALUE(Type, ...)                                                          \
	friend inline void to_json(::ptgn::json& nlohmann_json_j, const Type& nlohmann_json_t) {     \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_TO_VALUE, __VA_ARGS__))   \
	}                                                                                            \
	friend inline void from_json(const ::ptgn::json& nlohmann_json_j, Type& nlohmann_json_t) {   \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_FROM_VALUE, __VA_ARGS__)) \
	}

#define PTGN_SERIALIZE_EMPTY(Type)                                                              \
	friend inline void to_json(::ptgn::json& j, const Type&) {                                  \
		j = PTGN_STRINGIFY(Type);                                                               \
	}                                                                                           \
	friend inline void from_json(const ::ptgn::json& j, Type&) {                                \
		const auto s = j.get<std::string>();                                                    \
		if (s != PTGN_STRINGIFY(Type)) {                                                        \
			throw std::runtime_error(                                                           \
				"Invalid struct name found in JSON: " + s + ", expected: " PTGN_STRINGIFY(Type) \
			);                                                                                  \
		}                                                                                       \
	}

#define PTGN_SERIALIZE_DERIVED(Type, Base, ...)                                                \
	friend inline void to_json(::ptgn::json& nlohmann_json_j, const Type& nlohmann_json_t) {   \
		to_json(nlohmann_json_j, static_cast<const Base&>(nlohmann_json_t));                   \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_TO, __VA_ARGS__))       \
	}                                                                                          \
	friend inline void from_json(const ::ptgn::json& nlohmann_json_j, Type& nlohmann_json_t) { \
		from_json(nlohmann_json_j, static_cast<Base&>(nlohmann_json_t));                       \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_IMPL_EXTEND_JSON_FROM, __VA_ARGS__))     \
	}

/// @brief Use this OUTSIDE the enum declaration.
/// Declares JSON serialization for the enum.
#define PTGN_REFLECT_ENUM(Type) PTGN_SERIALIZE_ENUM(Type)

/// @brief Use this OUTSIDE the enum declaration.
/// Declares JSON serialization for the enum using an explicit list of enum cases.
#define PTGN_REFLECT_ENUM_MANUAL(Type, ...) PTGN_SERIALIZE_ENUM_MANUAL(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares JSON serialization for the class/struct.
#define PTGN_REFLECT(Type, ...) PTGN_SERIALIZE(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares JSON serialization for the class/struct.
/// Serializes directly as that value, without a field name.
#define PTGN_REFLECT_VALUE(Type, Field) PTGN_SERIALIZE_VALUE(Type, Field)

/// @brief Use this INSIDE the class/struct body.
/// Declares JSON serialization for the class/struct.
/// Serializes directly as the name of the class/struct.
#define PTGN_REFLECT_EMPTY(Type) PTGN_SERIALIZE_EMPTY(Type)

/// @brief Use this INSIDE the class/struct body.
/// Declares JSON serialization for the class/struct and its base class.
#define PTGN_REFLECT_DERIVED(Type, Base, ...) PTGN_SERIALIZE_DERIVED(Type, Base, __VA_ARGS__)