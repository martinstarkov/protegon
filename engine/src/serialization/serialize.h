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
#include "core/util/concepts.h"
#include "core/util/macro.h"
#include "core/util/macro_loop.h"

namespace ptgn::impl {

constexpr std::string_view StripTrailingUnderscore(std::string_view name) {
	return (!name.empty() && name.back() == '_') ? name.substr(0, name.size() - 1) : name;
}

template <typename T>
concept OptionalType = SpecializationOf<T, std::optional>;

template <typename T>
void SerializeField(nlohmann::json& j, std::string_view key, const T& value) {
	if constexpr (OptionalType<T>) {
		if (value.has_value()) {
			j[std::string{ key }] = *value;
		}
	} else {
		j[std::string{ key }] = value;
	}
}

template <typename T>
void SerializeValue(nlohmann::json& j, const T& value) {
	if constexpr (OptionalType<T>) {
		if (value.has_value()) {
			j = *value;
		} else {
			j = nlohmann::json::value_t::null;
		}
	} else {
		j = value;
	}
}

template <typename T>
void DeserializeField(const nlohmann::json& j, std::string_view key, T& value) {
	if constexpr (OptionalType<T>) {
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
void DeserializeValue(const nlohmann::json& j, T& value) {
	if constexpr (OptionalType<T>) {
		if (j.is_null()) {
			value = std::nullopt;
		} else {
			value = j.template get<typename std::remove_cvref_t<T>::value_type>();
		}
	} else {
		j.get_to(value);
	}
}

template <typename T>
void StreamValue(std::ostream& os, const T& value);

template <typename T>
void StreamIterable(std::ostream& os, const T& value) {
	os << "[";
	bool first{ true };
	for (const auto& element : value) {
		if (!first) {
			os << ", ";
		}
		first = false;
		StreamValue(os, element);
	}
	os << "]";
}

template <typename T>
void StreamField(std::ostream& os, bool& first, std::string_view key, const T& value) {
	if constexpr (OptionalType<T>) {
		if (!value.has_value()) {
			return;
		}
		if (!first) {
			os << ", ";
		}
		first = false;
		os << key << ": ";
		StreamValue(os, *value);
	} else {
		if (!first) {
			os << ", ";
		}
		first = false;
		os << key << ": ";
		StreamValue(os, value);
	}
}

template <typename T>
void StreamValue(std::ostream& os, const T& value) {
	if constexpr (OptionalType<T>) {
		if (value.has_value()) {
			StreamValue(os, *value);
		} else {
			os << "null";
		}
	} else if constexpr (IterableType<T>) {
		StreamIterable(os, value);
	} else {
		static_assert(Streamable<T>, "Type must be streamable or iterable");
		os << value;
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

/// @brief Use this OUTSIDE the enum declaration.
#define PTGN_OSTREAM_ENUM(Type)                                                       \
	inline std::ostream& operator<<(std::ostream& os, Type value) {                   \
		static_assert(                                                                \
			std::is_enum_v<Type>,                                                     \
			"PTGN_OSTREAM_ENUM must be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                            \
		if (auto name{ magic_enum::enum_name(value) }; !name.empty()) {               \
			return os << name;                                                        \
		}                                                                             \
		return os << std::to_underlying(value);                                       \
	}

/// @brief Use this INSIDE the class/struct body.
#define PTGN_OSTREAM(Type, ...)                                                      \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {           \
		static_assert(                                                               \
			!std::is_enum_v<Type>,                                                   \
			"PTGN_OSTREAM must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                           \
		bool first{ true };                                                          \
		os << "{";                                                                   \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC, __VA_ARGS__)              \
		os << "}";                                                                   \
		return os;                                                                   \
	}

/// @brief Use this INSIDE the class/struct body.
/// Removes underscores from all member names.
#define PTGN_OSTREAM_PRIV(Type, ...)                                                      \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                \
		static_assert(                                                                    \
			!std::is_enum_v<Type>,                                                        \
			"PTGN_OSTREAM_PRIV must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                \
		bool first{ true };                                                               \
		os << "{";                                                                        \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PRIV, __VA_ARGS__)                     \
		os << "}";                                                                        \
		return os;                                                                        \
	}

/// @brief Use this INSIDE the class/struct body.
/// Prints directly as that value, without a field name.
#define PTGN_OSTREAM_VALUE(Type, Field)                                                    \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                 \
		static_assert(                                                                     \
			!std::is_enum_v<Type>,                                                         \
			"PTGN_OSTREAM_VALUE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                 \
		::ptgn::impl::StreamValue(os, value.Field);                                        \
		return os;                                                                         \
	}

#define PTGN_OSTREAM_DERIVED(Type, Base, ...)                                                \
	friend std::ostream& operator<<(std::ostream& os, const Type& value) {                   \
		bool first{ true };                                                                  \
		os << "{";                                                                           \
		[&] {                                                                                \
			std::ostringstream base_os;                                                      \
			base_os << static_cast<const Base&>(value);                                      \
			auto base_str = base_os.str();                                                   \
			if (base_str.size() >= 2 && base_str.front() == '{' && base_str.back() == '}') { \
				base_str = base_str.substr(1, base_str.size() - 2);                          \
			}                                                                                \
			if (!base_str.empty()) {                                                         \
				os << base_str;                                                              \
				first = false;                                                               \
			}                                                                                \
		}();                                                                                 \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD_PUBLIC, __VA_ARGS__)                      \
		os << "}";                                                                           \
		return os;                                                                           \
	}

/// @brief Use this OUTSIDE the enum declaration.
#define PTGN_SERIALIZE_ENUM(Type)                                                       \
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

/// @brief Use this INSIDE the class/struct body.
#define PTGN_SERIALIZE(Type, ...)                                                      \
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

/// @brief Use this INSIDE the class/struct body.
/// Removes underscores from all member names.
#define PTGN_SERIALIZE_PRIV(Type, ...)                                                      \
	friend void to_json(nlohmann::json& j, const Type& value) {                             \
		static_assert(                                                                      \
			!std::is_enum_v<Type>,                                                          \
			"PTGN_SERIALIZE_PRIV must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                  \
		j = nlohmann::json::object();                                                       \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PRIV, __VA_ARGS__)                       \
	}                                                                                       \
	friend void from_json(const nlohmann::json& j, Type& value) {                           \
		static_assert(                                                                      \
			!std::is_enum_v<Type>,                                                          \
			"PTGN_SERIALIZE_PRIV must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                  \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PRIV, __VA_ARGS__)                     \
	}

/// @brief Use this INSIDE the class/struct body.
/// Serializes directly as that value, without a field name.
#define PTGN_SERIALIZE_VALUE(Type, Field)                                                    \
	friend void to_json(nlohmann::json& j, const Type& value) {                              \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE_VALUE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                   \
		::ptgn::impl::SerializeValue(j, value.Field);                                        \
	}                                                                                        \
	friend void from_json(const nlohmann::json& j, Type& value) {                            \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE_VALUE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                   \
		::ptgn::impl::DeserializeValue(j, value.Field);                                      \
	}

#define PTGN_SERIALIZE_DERIVED(Type, Base, ...)                           \
	friend void to_json(nlohmann::json& j, const Type& value) {           \
		to_json(j, static_cast<const Base&>(value));                      \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD_PUBLIC, __VA_ARGS__)   \
	}                                                                     \
	friend void from_json(const nlohmann::json& j, Type& value) {         \
		from_json(j, static_cast<Base&>(value));                          \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD_PUBLIC, __VA_ARGS__) \
	}

/// @brief Use this OUTSIDE the enum declaration.
/// Declares both ostream operator and JSON serialization for the enum.
#define PTGN_REFLECT_ENUM(Type) \
	PTGN_OSTREAM_ENUM(Type)     \
	PTGN_SERIALIZE_ENUM(Type)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
#define PTGN_REFLECT(Type, ...)     \
	PTGN_OSTREAM(Type, __VA_ARGS__) \
	PTGN_SERIALIZE(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
/// Removes underscores from all member names.
#define PTGN_REFLECT_PRIV(Type, ...)     \
	PTGN_OSTREAM_PRIV(Type, __VA_ARGS__) \
	PTGN_SERIALIZE_PRIV(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
/// Serializes and prints directly as that value, without a field name.
#define PTGN_REFLECT_VALUE(Type, Field) \
	PTGN_OSTREAM_VALUE(Type, Field)     \
	PTGN_SERIALIZE_VALUE(Type, Field)

#define PTGN_REFLECT_DERIVED(Type, Base, ...)     \
	PTGN_OSTREAM_DERIVED(Type, Base, __VA_ARGS__) \
	PTGN_SERIALIZE_DERIVED(Type, Base, __VA_ARGS__)