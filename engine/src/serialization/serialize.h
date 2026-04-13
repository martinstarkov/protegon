#pragma once

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/log.h"
#include "core/util/macro.h"
#include "core/util/macro_loop.h"

namespace ptgn::impl {

constexpr std::string_view StripTrailingUnderscore(std::string_view name) {
	return (!name.empty() && name.back() == '_') ? name.substr(0, name.size() - 1) : name;
}

} // namespace ptgn::impl

// Helpers

#define PTGN_IMPL_SERIALIZE_PRIV_KEY(member) ::ptgn::impl::StripTrailingUnderscore(#member)

#define PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC(member, object, index) \
	if constexpr ((index) > 0) {                                        \
		os << ", ";                                                     \
	}                                                                   \
	os << PTGN_STRINGIFY(member) << ": " << (object).member;

#define PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PRIV(member, object, index) \
	if constexpr ((index) > 0) {                                      \
		os << ", ";                                                   \
	}                                                                 \
	os << PTGN_IMPL_SERIALIZE_PRIV_KEY(member) << ": " << (object).member;

#define PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PUBLIC(member, object) \
	{ PTGN_STRINGIFY(member), (object).member }

#define PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PRIV(member, object) \
	{ std::string{ PTGN_IMPL_SERIALIZE_PRIV_KEY(member) }, (object).member }

#define PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PUBLIC(member, object) \
	j.at(PTGN_STRINGIFY(member)).get_to((object).member);

#define PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PRIV(member, object) \
	j.at(std::string{ PTGN_IMPL_SERIALIZE_PRIV_KEY(member) }).get_to((object).member);

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

/// @brief Public struct/class serializer.
/// Use this INSIDE the struct/class body.
#define PTGN_SERIALIZE(Type, ...)                                                            \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                   \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type)       \
		);                                                                                   \
		os << "{";                                                                           \
		PTGN_MAP_DATA_INDEX(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC, value, __VA_ARGS__)    \
		os << "}";                                                                           \
		return os;                                                                           \
	}                                                                                        \
	friend void to_json(nlohmann::json& j, const Type& value) {                              \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type)       \
		);                                                                                   \
		j = nlohmann::json{                                                                  \
			PTGN_MAP_LIST_DATA(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PUBLIC, value, __VA_ARGS__) \
		};                                                                                   \
	}                                                                                        \
	friend void from_json(const nlohmann::json& j, Type& value) {                            \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type)       \
		);                                                                                   \
		PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PUBLIC, value, __VA_ARGS__)        \
	}

/// @brief Private struct/class serializer (removes underscores from all member names).
/// Use this INSIDE the struct/class body.
#define PTGN_SERIALIZE_PRIV(Type, ...)                                                      \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                  \
		static_assert(                                                                      \
			!std::is_enum_v<Type>,                                                          \
			"PTGN_SERIALIZE_PRIV must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                  \
		os << "{";                                                                          \
		PTGN_MAP_DATA_INDEX(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PRIV, value, __VA_ARGS__)     \
		os << "}";                                                                          \
		return os;                                                                          \
	}                                                                                       \
	friend void to_json(nlohmann::json& j, const Type& value) {                             \
		static_assert(                                                                      \
			!std::is_enum_v<Type>, "PTGN_SERIALIZE_PRIV must not be used with an enum type" \
		);                                                                                  \
		j = nlohmann::json{                                                                 \
			PTGN_MAP_LIST_DATA(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PRIV, value, __VA_ARGS__)  \
		};                                                                                  \
	}                                                                                       \
	friend void from_json(const nlohmann::json& j, Type& value) {                           \
		static_assert(                                                                      \
			!std::is_enum_v<Type>, "PTGN_SERIALIZE_PRIV must not be used with an enum type" \
		);                                                                                  \
		PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PRIV, value, __VA_ARGS__)         \
	}