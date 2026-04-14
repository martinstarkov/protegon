#pragma once

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "core/log.h"
#include "core/util/macro.h"
#include "core/util/macro_loop.h"

namespace ptgn::impl {

constexpr std::string_view StripTrailingUnderscore(std::string_view name) {
	return (!name.empty() && name.back() == '_') ? name.substr(0, name.size() - 1) : name;
}

template <typename T>
struct IsOptional : std::false_type {};

template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool kIsOptional = IsOptional<std::remove_cvref_t<T>>::value;

template <typename T>
void SerializeField(nlohmann::json& j, std::string_view key, const T& value) {
	if constexpr (kIsOptional<T>) {
		if (value.has_value()) {
			j[std::string{ key }] = *value;
		}
	} else {
		j[std::string{ key }] = value;
	}
}

template <typename T>
void DeserializeField(const nlohmann::json& j, std::string_view key, T& value) {
	if constexpr (kIsOptional<T>) {
		auto it = j.find(std::string{ key });
		if (it == j.end() || it->is_null()) {
			value = std::nullopt;
		} else {
			value = it->template get<typename std::remove_cvref_t<T>::value_type>();
		}
	} else {
		j.at(std::string{ key }).get_to(value);
	}
}

template <typename T>
void StreamField(std::ostream& os, bool& first, std::string_view key, const T& value) {
	if constexpr (kIsOptional<T>) {
		if (!value.has_value()) {
			return;
		}
		if (!first) {
			os << ", ";
		}
		first = false;
		os << key << ": " << *value;
	} else {
		if (!first) {
			os << ", ";
		}
		first = false;
		os << key << ": " << value;
	}
}

} // namespace ptgn::impl

// Helpers

#define PTGN_IMPL_SERIALIZE_PRIV_KEY(member) ::ptgn::impl::StripTrailingUnderscore(#member)

#define PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC(member) \
	::ptgn::impl::StreamField(os, first, PTGN_STRINGIFY(member), value.member);

#define PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PRIV(member) \
	::ptgn::impl::StreamField(os, first, PTGN_IMPL_SERIALIZE_PRIV_KEY(member), value.member);

#define PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PUBLIC(member) \
	::ptgn::impl::SerializeField(j, PTGN_STRINGIFY(member), value.member);

#define PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PRIV(member) \
	::ptgn::impl::SerializeField(j, PTGN_IMPL_SERIALIZE_PRIV_KEY(member), value.member);

#define PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PUBLIC(member) \
	::ptgn::impl::DeserializeField(j, PTGN_STRINGIFY(member), value.member);

#define PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PRIV(member) \
	::ptgn::impl::DeserializeField(j, PTGN_IMPL_SERIALIZE_PRIV_KEY(member), value.member);

#define PTGN_SERIALIZE_ENUM_NOSTREAM(Type)                                              \
	inline void to_json(nlohmann::json& j, Type value) {                                \
		static_assert(                                                                  \
			std::is_enum_v<Type>,                                                       \
			"PTGN_SERIALIZE_ENUM must be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                              \
		if (auto name{ magic_enum::enum_name(value) }; !name.empty()) {                 \
			j = std::string{ name };                                                    \
			return;                                                                     \
		}                                                                               \
		j = std::to_underlying(value);                                                  \
	}                                                                                   \
	inline void from_json(const nlohmann::json& j, Type& value) {                       \
		static_assert(                                                                  \
			std::is_enum_v<Type>,                                                       \
			"PTGN_SERIALIZE_ENUM must be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                              \
		if (j.is_string()) {                                                            \
			const auto s{ j.get<std::string>() };                                       \
			if (auto parsed{ magic_enum::enum_cast<Type>(s) }; parsed.has_value()) {    \
				value = *parsed;                                                        \
				return;                                                                 \
			}                                                                           \
			PTGN_ERROR("Invalid enum name in JSON: ", s);                               \
		}                                                                               \
		value = static_cast<Type>(j.get<std::underlying_type_t<Type>>());               \
	}

/// @brief Enum serializer.
/// Use this OUTSIDE the enum declaration.
#define PTGN_SERIALIZE_ENUM(Type)                                                       \
	inline std::ostream& operator<<(std::ostream& os, Type value) {                     \
		static_assert(                                                                  \
			std::is_enum_v<Type>,                                                       \
			"PTGN_SERIALIZE_ENUM must be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                              \
		if (auto name{ magic_enum::enum_name(value) }; !name.empty()) {                 \
			return os << name;                                                          \
		}                                                                               \
		return os << std::to_underlying(value);                                         \
	}                                                                                   \
	PTGN_SERIALIZE_ENUM_NOSTREAM(Type)

/// @brief Public struct/class serializer.
/// Use this INSIDE the struct/class body.
#define PTGN_SERIALIZE(Type, ...)                                                      \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {             \
		static_assert(                                                                 \
			!std::is_enum_v<Type>,                                                     \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                             \
		bool first{ true };                                                            \
		os << "{";                                                                     \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC, __VA_ARGS__)                \
		os << "}";                                                                     \
		return os;                                                                     \
	}                                                                                  \
	friend void to_json(nlohmann::json& j, const Type& value) {                        \
		static_assert(                                                                 \
			!std::is_enum_v<Type>,                                                     \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                             \
		j = nlohmann::json::object();                                                  \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PUBLIC, __VA_ARGS__)                \
	}                                                                                  \
	friend void from_json(const nlohmann::json& j, Type& value) {                      \
		static_assert(                                                                 \
			!std::is_enum_v<Type>,                                                     \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                             \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PUBLIC, __VA_ARGS__)              \
	}

/// @brief Private struct/class serializer (removes underscores from all member names).
/// Use this INSIDE the struct/class body.
#define PTGN_SERIALIZE_PRIV(Type, ...)                                                      \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                  \
		static_assert(                                                                      \
			!std::is_enum_v<Type>,                                                          \
			"PTGN_SERIALIZE_PRIV must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                  \
		bool first{ true };                                                                 \
		os << "{";                                                                          \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PRIV, __VA_ARGS__)                       \
		os << "}";                                                                          \
		return os;                                                                          \
	}                                                                                       \
	friend void to_json(nlohmann::json& j, const Type& value) {                             \
		static_assert(                                                                      \
			!std::is_enum_v<Type>, "PTGN_SERIALIZE_PRIV must not be used with an enum type" \
		);                                                                                  \
		j = nlohmann::json::object();                                                       \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PRIV, __VA_ARGS__)                       \
	}                                                                                       \
	friend void from_json(const nlohmann::json& j, Type& value) {                           \
		static_assert(                                                                      \
			!std::is_enum_v<Type>, "PTGN_SERIALIZE_PRIV must not be used with an enum type" \
		);                                                                                  \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PRIV, __VA_ARGS__)                     \
	}