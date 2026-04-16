#pragma once

#include <array>
#include <concepts>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/detail/iterators/iter_impl.hpp>
#include <nlohmann/detail/value_t.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/concepts.h"
#include "core/util/macro.h"
#include "core/util/macro_loop.h"
#include "serialization/json/json.h"

namespace ptgn {

template <typename TVariant>
struct VariantNames;

namespace impl {

template <typename T, typename = void>
struct has_variant_names : std::false_type {};

template <typename T>
struct has_variant_names<T, std::void_t<decltype(VariantNames<T>::names)>> : std::true_type {};

template <typename T>
concept HasVariantNames = has_variant_names<T>::value;

constexpr std::string_view StripTrailingUnderscore(std::string_view name) {
	return (!name.empty() && name.back() == '_') ? name.substr(0, name.size() - 1) : name;
}

template <typename T>
concept OptionalType = SpecializationOf<T, std::optional>;

template <typename T>
void SerializeValue(json& j, const T& value);

template <typename T>
void DeserializeValue(const json& j, T& value);

template <typename Variant, std::size_t... Is>
constexpr std::string_view VariantTypeNameByIndex(std::size_t index, std::index_sequence<Is...>) {
	std::string_view result{};

	(
		[&] {
			if (index == Is) {
				result = VariantNames<Variant>::names[Is];
			}
		}(),
		...
	);

	return result;
}

template <std::size_t I = 0, typename... Ts>
void DeserializeVariantByName(
	const json& value_json, std::string_view type, std::variant<Ts...>& value
) {
	using Variant = std::variant<Ts...>;

	if constexpr (I >= sizeof...(Ts)) {
		PTGN_ASSERT(false, "Invalid variant type: ", type);
	} else {
		if (type == VariantNames<Variant>::names[I]) {
			using Alt = std::variant_alternative_t<I, Variant>;

			if constexpr (std::default_initializable<Alt>) {
				Alt temp{};
				DeserializeValue(value_json, temp);
				value = std::move(temp);
			} else {
				value = value_json.template get<Alt>();
			}
		} else {
			DeserializeVariantByName<I + 1>(value_json, type, value);
		}
	}
}

template <typename... Ts>
void SerializeVariant(json& j, const std::variant<Ts...>& value) {
	using Variant = std::variant<Ts...>;
	static_assert(
		HasVariantNames<Variant>, "VariantNames specialization is required for this variant type"
	);

	j		  = json::object();
	j["type"] = std::string{
		VariantTypeNameByIndex<Variant>(value.index(), std::make_index_sequence<sizeof...(Ts)>{})
	};

	std::visit([&]<typename TAlt>(const TAlt& alt) { SerializeValue(j["value"], alt); }, value);
}

template <typename... Ts>
void DeserializeVariant(const json& j, std::variant<Ts...>& value) {
	using Variant = std::variant<Ts...>;
	static_assert(
		HasVariantNames<Variant>, "VariantNames specialization is required for this variant type"
	);

	PTGN_ASSERT(j.is_object(), "Expected object for variant deserialization");

	const auto type_name   = j.at("type").template get<std::string>();
	const auto& value_json = j.at("value");

	DeserializeVariantByName(value_json, type_name, value);
}

template <typename T>
void SerializeField(json& j, std::string_view key, const T& value) {
	if constexpr (OptionalType<T>) {
		if (value.has_value()) {
			SerializeValue(j[std::string{ key }], *value);
		}
	} else {
		SerializeValue(j[std::string{ key }], value);
	}
}

template <typename T>
void SerializeValue(json& j, const T& value) {
	if constexpr (OptionalType<T>) {
		if (value.has_value()) {
			SerializeValue(j, *value);
		} else {
			j = json::value_t::null;
		}
	} else if constexpr (VariantType<T>) {
		SerializeVariant(j, value);
	} else {
		j = value;
	}
}

template <typename T>
void DeserializeField(const json& j, std::string_view key, T& value) {
	if constexpr (OptionalType<T>) {
		auto it = j.find(std::string{ key });
		if (it == j.end() || it->is_null()) {
			value = std::nullopt;
		} else {
			value.emplace();
			DeserializeValue(*it, *value);
		}
	} else {
		DeserializeValue(j.at(std::string{ key }), value);
	}
}

template <typename T>
void DeserializeValue(const json& j, T& value) {
	if constexpr (OptionalType<T>) {
		if (j.is_null()) {
			value = std::nullopt;
		} else {
			value.emplace();
			DeserializeValue(j, *value);
		}
	} else if constexpr (VariantType<T>) {
		DeserializeVariant(j, value);
	} else {
		j.get_to(value);
	}
}

template <typename T>
void StreamValue(std::ostream& os, const T& value);

template <typename... Ts>
void StreamVariant(std::ostream& os, const std::variant<Ts...>& value) {
	std::visit([&]<typename TAlt>(const TAlt& alt) { StreamValue(os, alt); }, value);
}

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
	} else if constexpr (std::ranges::range<T> && !std::is_convertible_v<T, std::string_view>) {
		StreamIterable(os, value);
	} else if constexpr (VariantType<T>) {
		StreamVariant(os, value);
	} else {
		static_assert(StreamWritable<T>, "Type must be stream writable");
		os << value;
	}
}

} // namespace impl

} // namespace ptgn

// Helpers

#define PTGN_IMPL_SERIALIZE_OSTREAM_FIELD(member)                               \
	::ptgn::impl::StreamField(                                                  \
		os, first, ::ptgn::impl::StripTrailingUnderscore(#member), value.member \
	);

#define PTGN_IMPL_SERIALIZE_TO_JSON_FIELD(member) \
	::ptgn::impl::SerializeField(j, ::ptgn::impl::StripTrailingUnderscore(#member), value.member);

#define PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD(member) \
	::ptgn::impl::DeserializeField(j, ::ptgn::impl::StripTrailingUnderscore(#member), value.member);

#define PTGN_IMPL_OSTREAM_ENUM_CASE(EnumCase, Type) \
	case Type::EnumCase: return os << PTGN_STRINGIFY(EnumCase);

#define PTGN_IMPL_SERIALIZE_ENUM_TO_JSON_CASE(EnumCase, Type) \
	case Type::EnumCase: j = PTGN_STRINGIFY(EnumCase); return;

#define PTGN_IMPL_SERIALIZE_ENUM_FROM_JSON_CASE(EnumCase, Type) \
	if (s == PTGN_STRINGIFY(EnumCase)) {                        \
		value = Type::EnumCase;                                 \
		return;                                                 \
	}

/// @brief Macro for defining a specialization of VariantNames for a specific variant type,
/// providing the names of the variants in order.
#define PTGN_VARIANT_NAMES(VariantType, ...)                                 \
	template <>                                                              \
	struct VariantNames<PTGN_UNPAREN VariantType> {                          \
		static constexpr std::array<                                         \
			std::string_view, std::variant_size_v<PTGN_UNPAREN VariantType>> \
			names{ __VA_ARGS__ };                                            \
	}

/// @brief Macro for defining a specialization of VariantNames for a specific variant type,
/// providing the names of the variants in order.
/// Supports one template parameter.
#define PTGN_VARIANT_NAMES_TEMPLATE(TParam, VariantType, ...)                \
	template <typename TParam>                                               \
	struct VariantNames<PTGN_UNPAREN VariantType> {                          \
		static constexpr std::array<                                         \
			std::string_view, std::variant_size_v<PTGN_UNPAREN VariantType>> \
			names{ __VA_ARGS__ };                                            \
	}

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
		PTGN_ERROR("Unknown " PTGN_STRINGIFY(Type) ": ", std::to_underlying(value));  \
	}

/// @brief Use this OUTSIDE the enum declaration.
/// Declares ostream operator for an enum using an explicit list of enum cases.
#define PTGN_OSTREAM_ENUM_MANUAL(Type, ...)                                                       \
	inline std::ostream& operator<<(std::ostream& os, Type value) {                               \
		static_assert(                                                                            \
			std::is_enum_v<Type>,                                                                 \
			"PTGN_OSTREAM_ENUM_MANUAL must be used with an enum type: " PTGN_STRINGIFY(Type)      \
		);                                                                                        \
		switch (value) {                                                                          \
			PTGN_MAP_DATA(PTGN_IMPL_OSTREAM_ENUM_CASE, Type, __VA_ARGS__)                         \
			default: PTGN_ERROR("Unknown " PTGN_STRINGIFY(Type) ": ", std::to_underlying(value)); \
		}                                                                                         \
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
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD, __VA_ARGS__)                     \
		os << "}";                                                                   \
		return os;                                                                   \
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

#define PTGN_OSTREAM_EMPTY(Type)                                                                \
	friend std::ostream& operator<<(std::ostream& os, const Type&) {                            \
		static_assert(                                                                          \
			!std::is_enum_v<Type> && std::is_empty_v<Type>,                                     \
			"PTGN_OSTREAM_EMPTY must be used with an empty class/struct: " PTGN_STRINGIFY(Type) \
		);                                                                                      \
		os << PTGN_STRINGIFY(Type);                                                             \
		return os;                                                                              \
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
		PTGN_MAP(PTGN_IMPL_SERIALIZE_OSTREAM_FIELD, __VA_ARGS__)                             \
		os << "}";                                                                           \
		return os;                                                                           \
	}

/// @brief Use this OUTSIDE the enum declaration.
#define PTGN_SERIALIZE_ENUM(Type)                                                       \
	inline void to_json(json& j, Type value) {                                          \
		static_assert(                                                                  \
			std::is_enum_v<Type>,                                                       \
			"PTGN_SERIALIZE_ENUM must be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                              \
		if (auto name{ magic_enum::enum_name(value) }; !name.empty()) {                 \
			j = std::string{ name };                                                    \
			return;                                                                     \
		}                                                                               \
		PTGN_ERROR("Unknown " PTGN_STRINGIFY(Type) ": ", std::to_underlying(value));    \
	}                                                                                   \
	inline void from_json(const json& j, Type& value) {                                 \
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

/// @brief Use this OUTSIDE the enum declaration.
/// Declares JSON serialization for an enum using an explicit list of enum cases.
#define PTGN_SERIALIZE_ENUM_MANUAL(Type, ...)                                                     \
	inline void to_json(json& j, Type value) {                                                    \
		static_assert(                                                                            \
			std::is_enum_v<Type>,                                                                 \
			"PTGN_SERIALIZE_ENUM_MANUAL must be used with an enum type: " PTGN_STRINGIFY(Type)    \
		);                                                                                        \
		switch (value) {                                                                          \
			PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_ENUM_TO_JSON_CASE, Type, __VA_ARGS__)               \
			default: PTGN_ERROR("Unknown " PTGN_STRINGIFY(Type) ": ", std::to_underlying(value)); \
		}                                                                                         \
	}                                                                                             \
	inline void from_json(const json& j, Type& value) {                                           \
		static_assert(                                                                            \
			std::is_enum_v<Type>,                                                                 \
			"PTGN_SERIALIZE_ENUM_MANUAL must be used with an enum type: " PTGN_STRINGIFY(Type)    \
		);                                                                                        \
		if (j.is_string()) {                                                                      \
			const auto s{ j.get<std::string>() };                                                 \
			PTGN_MAP_DATA(PTGN_IMPL_SERIALIZE_ENUM_FROM_JSON_CASE, Type, __VA_ARGS__)             \
			PTGN_ERROR("Invalid enum name in JSON: ", s);                                         \
		}                                                                                         \
		value = static_cast<Type>(j.get<std::underlying_type_t<Type>>());                         \
	}

/// @brief Use this INSIDE the class/struct body.
#define PTGN_SERIALIZE(Type, ...)                                                      \
	friend void to_json(json& j, const Type& value) {                                  \
		static_assert(                                                                 \
			!std::is_enum_v<Type>,                                                     \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                             \
		j = json::object();                                                            \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD, __VA_ARGS__)                       \
	}                                                                                  \
	friend void from_json(const json& j, Type& value) {                                \
		static_assert(                                                                 \
			!std::is_enum_v<Type>,                                                     \
			"PTGN_SERIALIZE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                             \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD, __VA_ARGS__)                     \
	}

/// @brief Use this INSIDE the class/struct body.
/// Serializes directly as that value, without a field name.
#define PTGN_SERIALIZE_VALUE(Type, Field)                                                    \
	friend void to_json(json& j, const Type& value) {                                        \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE_VALUE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                   \
		::ptgn::impl::SerializeValue(j, value.Field);                                        \
	}                                                                                        \
	friend void from_json(const json& j, Type& value) {                                      \
		static_assert(                                                                       \
			!std::is_enum_v<Type>,                                                           \
			"PTGN_SERIALIZE_VALUE must not be used with an enum type: " PTGN_STRINGIFY(Type) \
		);                                                                                   \
		::ptgn::impl::DeserializeValue(j, value.Field);                                      \
	}

#define PTGN_SERIALIZE_EMPTY(Type)                                                                \
	friend void to_json(json& j, const Type&) {                                                   \
		static_assert(                                                                            \
			!std::is_enum_v<Type> && std::is_empty_v<Type>,                                       \
			"PTGN_SERIALIZE_EMPTY must be used with an empty class/struct: " PTGN_STRINGIFY(Type) \
		);                                                                                        \
		j = PTGN_STRINGIFY(Type);                                                                 \
	}                                                                                             \
	friend void from_json(const json& j, Type&) {                                                 \
		static_assert(                                                                            \
			!std::is_enum_v<Type> && std::is_empty_v<Type>,                                       \
			"PTGN_SERIALIZE_EMPTY must be used with an empty class/struct: " PTGN_STRINGIFY(Type) \
		);                                                                                        \
		const auto s = j.get<std::string>();                                                      \
		PTGN_ASSERT(s == PTGN_STRINGIFY(Type), "Expected ", s, " for ", PTGN_STRINGIFY(Type));    \
	}

#define PTGN_SERIALIZE_DERIVED(Type, Base, ...)                    \
	friend void to_json(json& j, const Type& value) {              \
		to_json(j, static_cast<const Base&>(value));               \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_TO_JSON_FIELD, __VA_ARGS__)   \
	}                                                              \
	friend void from_json(const json& j, Type& value) {            \
		from_json(j, static_cast<Base&>(value));                   \
		PTGN_MAP(PTGN_IMPL_SERIALIZE_FROM_JSON_FIELD, __VA_ARGS__) \
	}

/// @brief Use this OUTSIDE the enum declaration.
/// Declares both ostream operator and JSON serialization for the enum.
#define PTGN_REFLECT_ENUM(Type) \
	PTGN_OSTREAM_ENUM(Type)     \
	PTGN_SERIALIZE_ENUM(Type)

/// @brief Use this OUTSIDE the enum declaration.
/// Declares both ostream operator and JSON serialization for the enum
/// using an explicit list of enum cases.
#define PTGN_REFLECT_ENUM_MANUAL(Type, ...)     \
	PTGN_OSTREAM_ENUM_MANUAL(Type, __VA_ARGS__) \
	PTGN_SERIALIZE_ENUM_MANUAL(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
#define PTGN_REFLECT(Type, ...)     \
	PTGN_OSTREAM(Type, __VA_ARGS__) \
	PTGN_SERIALIZE(Type, __VA_ARGS__)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
/// Serializes and prints directly as that value, without a field name.
#define PTGN_REFLECT_VALUE(Type, Field) \
	PTGN_OSTREAM_VALUE(Type, Field)     \
	PTGN_SERIALIZE_VALUE(Type, Field)

/// @brief Use this INSIDE the class/struct body.
/// Declares both ostream operator and JSON serialization for the class/struct.
/// Serializes and prints directly as the name of the class/struct.
#define PTGN_REFLECT_EMPTY(Type) \
	PTGN_OSTREAM_EMPTY(Type)     \
	PTGN_SERIALIZE_EMPTY(Type)

#define PTGN_REFLECT_DERIVED(Type, Base, ...)     \
	PTGN_OSTREAM_DERIVED(Type, Base, __VA_ARGS__) \
	PTGN_SERIALIZE_DERIVED(Type, Base, __VA_ARGS__)