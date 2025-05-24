#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/detail/meta/type_traits.hpp>
#include <string_view>

#include "serialization/json.h"

namespace ptgn {

namespace impl {

template <typename T>
struct JsonKeyValuePair {
	std::string_view key;
	T& value;

	JsonKeyValuePair(std::string_view json_key, T& json_value) :
		key{ json_key }, value{ json_value } {}
};

template <typename>
struct is_json_pair : public std::false_type {};

template <typename T>
struct is_json_pair<JsonKeyValuePair<T>> : public std::true_type {};

template <typename T>
inline constexpr bool is_json_pair_v{ is_json_pair<T>::value };

} // namespace impl

template <typename T>
impl::JsonKeyValuePair<T> KeyValue(std::string_view key, T& value) {
	return { key, value };
}

} // namespace ptgn

#define PTGN_KEY_VALUE_TO_JSON(kv) nlohmann_json_j[kv.key] = kv.value;

#define PTGN_KEY_VALUE_FROM_JSON(kv) nlohmann_json_j.at(kv.key).get_to(kv.value);

#define PTGN_SERIALIZER_REGISTER(Type, ...) \
	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)

// Must be placed in public field of class.
#define PTGN_SERIALIZER_REGISTER_NAMED(Type, ...)                                        \
private:                                                                                 \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	void local_to_json_impl(BasicJsonType& nlohmann_json_j) const {                      \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_KEY_VALUE_TO_JSON, __VA_ARGS__))   \
	}                                                                                    \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	void local_from_json_impl(const BasicJsonType& nlohmann_json_j) {                    \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_KEY_VALUE_FROM_JSON, __VA_ARGS__)) \
	}                                                                                    \
                                                                                         \
public:                                                                                  \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) {   \
		nlohmann_json_t.local_to_json_impl(nlohmann_json_j);                             \
	}                                                                                    \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { \
		nlohmann_json_t.local_from_json_impl(nlohmann_json_j);                           \
	}

#define PTGN_SERIALIZER_REGISTER_NAMELESS(Type, member)                                  \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) {   \
		nlohmann_json_j = nlohmann_json_t.member;                                        \
	}                                                                                    \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { \
		nlohmann_json_j.get_to(nlohmann_json_t.member);                                  \
	}
