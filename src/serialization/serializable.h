#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/detail/meta/type_traits.hpp>
#include <string_view>

#include "common/macro.h"
#include "common/type_traits.h"
#include "serialization/json.h"

namespace ptgn {

namespace impl {

template <class T, class R, class... Args>
std::is_convertible<std::invoke_result_t<T, Args...>, R> is_invokable_test(int);

template <class T, class R, class... Args>
std::false_type is_invokable_test(...);

template <class T, class R, class... Args>
using is_invokable = decltype(is_invokable_test<T, R, Args...>(0));

template <class T, class R, class... Args>
constexpr auto is_invokable_v = is_invokable<T, R, Args...>::value;

template <class L, class R = L>
using has_equality = is_invokable<std::equal_to<>, bool, L, R>;
template <class L, class R = L>
constexpr auto has_equality_v = has_equality<L, R>::value;

template <typename T>
struct JsonKeyValuePair {
	std::string_view key;
	T& value;

	[[nodiscard]] bool IsDefault() const {
		return false;
	}

	JsonKeyValuePair(std::string_view json_key, T& json_value) :
		key{ json_key }, value{ json_value } {}
};

template <typename T, typename S = T>
struct JsonKeyValuePairWithDefault {
	std::string_view key;
	T& value;
	S default_value;
	bool set_default{ false };

	[[nodiscard]] bool IsDefault() const {
		if constexpr (has_equality_v<T, S>) {
			if (set_default) {
				return value == default_value;
			}
		}
		return false;
	}

	JsonKeyValuePairWithDefault(
		std::string_view json_key, T& json_value, bool set_default, S default_val
	) :
		key{ json_key },
		value{ json_value },
		set_default{ set_default },
		default_value{ default_val } {}
};

template <typename>
struct is_json_pair : public std::false_type {};

template <typename T>
struct is_json_pair<JsonKeyValuePair<T>> : public std::true_type {};

template <typename T, typename S>
struct is_json_pair<JsonKeyValuePairWithDefault<T, S>> : public std::true_type {};

template <typename T, typename S = T>
inline constexpr bool is_json_pair_v{ is_json_pair<T>::value || is_json_pair<T, S>::value };

template <typename T, typename S>
impl::JsonKeyValuePairWithDefault<T, S> MakeKeyValue(
	std::string_view key, T& value, bool set_default, S default_value
) {
	return { key, value, set_default, default_value };
}

template <typename T>
impl::JsonKeyValuePair<T> MakeKeyValue(std::string_view key, T& value) {
	return { key, value };
}

template <typename T, typename S>
[[nodiscard]] bool CompareValues(const T& a, const S& b) {
	if constexpr (has_equality_v<T, S>) {
		return a == b;
	} else {
		return false;
	}
}

} // namespace impl

} // namespace ptgn

#define PTGN_GET_MACRO(_1, _2, _3, NAME, ...) NAME

#define PTGN_KEY_VALUE_IGNORE_DEFAULTS(json_key, member, ignore_defaults)          \
	ptgn::impl::MakeKeyValue(                                                      \
		json_key, member,                                                          \
		std::is_default_constructible_v<std::remove_reference_t<decltype(*this)>>, \
		std::is_default_constructible_v<std::remove_reference_t<decltype(*this)>>  \
			? std::remove_reference_t<decltype(*this)>{}.member                    \
			: std::remove_reference_t<decltype(member)>{}                          \
	)

#define PTGN_KEY_VALUE(json_key, member) ptgn::impl::MakeKeyValue(json_key, member)

#define KeyValue(...)                                                                            \
	PTGN_EXPAND(                                                                                 \
		PTGN_GET_MACRO(__VA_ARGS__, PTGN_KEY_VALUE_IGNORE_DEFAULTS, PTGN_KEY_VALUE)(__VA_ARGS__) \
	)

#define PTGN_TO_JSON_COMPARE(member)                                                    \
	if constexpr (ptgn::impl::has_equality_v<                                           \
					  std::remove_reference_t<decltype(nlohmann_json_t.member)>,        \
					  std::remove_reference_t<decltype(default_value.member)>>) {       \
		if (!ptgn::impl::CompareValues(nlohmann_json_t.member, default_value.member)) { \
			nlohmann_json_j[#member] = nlohmann_json_t.member;                          \
		}                                                                               \
	} else {                                                                            \
		nlohmann_json_j[#member] = nlohmann_json_t.member;                              \
	}

#define PTGN_KEY_VALUE_TO_JSON(kv) nlohmann_json_j[kv.key] = kv.value;

#define PTGN_KEY_VALUE_TO_JSON_COMPARE(kv)  \
	if (!kv.IsDefault()) {                  \
		nlohmann_json_j[kv.key] = kv.value; \
	}

#define PTGN_KEY_VALUE_FROM_JSON(kv)                                                              \
	if (auto nlohmann_json_j_value{ nlohmann_json_j.contains(kv.key) ? nlohmann_json_j.at(kv.key) \
																	 : json{} };                  \
		nlohmann_json_j_value.empty()) {                                                          \
		kv.value = {};                                                                            \
	} else {                                                                                      \
		nlohmann_json_j_value.get_to(kv.value);                                                   \
	}

#define PTGN_TO_JSON(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#define PTGN_FROM_JSON(v1)                                                                  \
	if (auto nlohmann_json_j_value{ nlohmann_json_j.contains(#v1) ? nlohmann_json_j.at(#v1) \
																  : json{} };               \
		nlohmann_json_j_value.empty()) {                                                    \
		nlohmann_json_t.v1 = {};                                                            \
	} else {                                                                                \
		nlohmann_json_j_value.get_to(nlohmann_json_t.v1);                                   \
	}
#define PTGN_FROM_JSON_WITH_DEFAULT(v1)                                                     \
	if (auto nlohmann_json_j_value{ nlohmann_json_j.contains(#v1) ? nlohmann_json_j.at(#v1) \
																  : json{} };               \
		nlohmann_json_j_value.empty()) {                                                    \
		nlohmann_json_t.v1 = {};                                                            \
	} else {                                                                                \
		nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1);      \
	}

#define PTGN_SERIALIZER_REGISTER(Type, ...)                                                 \
	friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) {     \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_TO_JSON, __VA_ARGS__))                \
	}                                                                                       \
	friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) {   \
		const Type nlohmann_json_default_obj{};                                             \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_FROM_JSON_WITH_DEFAULT, __VA_ARGS__)) \
	}

#define PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Type, ...)                                 \
	friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) {     \
		if constexpr (std::is_default_constructible_v<Type>) {                              \
			const Type default_value{};                                                     \
			(void)default_value;                                                            \
			NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_TO_JSON_COMPARE, __VA_ARGS__))    \
		} else {                                                                            \
			NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_TO_JSON, __VA_ARGS__))            \
		}                                                                                   \
	}                                                                                       \
	friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) {   \
		const Type nlohmann_json_default_obj{};                                             \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_FROM_JSON_WITH_DEFAULT, __VA_ARGS__)) \
	}

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

#define PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(Type, ...)                                  \
private:                                                                                           \
	template <                                                                                     \
		typename BasicJsonType,                                                                    \
		nlohmann::detail::enable_if_t<                                                             \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>                       \
	void local_to_json_impl(BasicJsonType& nlohmann_json_j) const {                                \
		if constexpr (std::is_default_constructible_v<Type>) {                                     \
			NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_KEY_VALUE_TO_JSON_COMPARE, __VA_ARGS__)) \
		} else {                                                                                   \
			NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_KEY_VALUE_TO_JSON, __VA_ARGS__))         \
		}                                                                                          \
	}                                                                                              \
	template <                                                                                     \
		typename BasicJsonType,                                                                    \
		nlohmann::detail::enable_if_t<                                                             \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>                       \
	void local_from_json_impl(const BasicJsonType& nlohmann_json_j) {                              \
		NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(PTGN_KEY_VALUE_FROM_JSON, __VA_ARGS__))           \
	}                                                                                              \
                                                                                                   \
public:                                                                                            \
	template <                                                                                     \
		typename BasicJsonType,                                                                    \
		nlohmann::detail::enable_if_t<                                                             \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>                       \
	friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) {             \
		nlohmann_json_t.local_to_json_impl(nlohmann_json_j);                                       \
	}                                                                                              \
	template <                                                                                     \
		typename BasicJsonType,                                                                    \
		nlohmann::detail::enable_if_t<                                                             \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>                       \
	friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) {           \
		nlohmann_json_t.local_from_json_impl(nlohmann_json_j);                                     \
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
		if (nlohmann_json_j.empty()) {                                                   \
			nlohmann_json_t = {};                                                        \
		} else {                                                                         \
			nlohmann_json_j.get_to(nlohmann_json_t.member);                              \
		}                                                                                \
	}

#define PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(Type, member)                  \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void to_json(BasicJsonType& nlohmann_json_j, const Type& nlohmann_json_t) {   \
		if constexpr (ptgn::impl::has_equality_v<                                        \
						  std::remove_reference_t<decltype(nlohmann_json_t.member)>,     \
						  std::remove_reference_t<decltype(Type{}.member)>> &&           \
					  std::is_default_constructible_v<Type>) {                           \
			if (!(nlohmann_json_t.member == Type{}.member)) {                            \
				nlohmann_json_j = nlohmann_json_t.member;                                \
			}                                                                            \
		} else {                                                                         \
			nlohmann_json_j = nlohmann_json_t.member;                                    \
		}                                                                                \
	}                                                                                    \
	template <                                                                           \
		typename BasicJsonType,                                                          \
		nlohmann::detail::enable_if_t<                                                   \
			nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>             \
	friend void from_json(const BasicJsonType& nlohmann_json_j, Type& nlohmann_json_t) { \
		if (nlohmann_json_j.empty()) {                                                   \
			nlohmann_json_t = {};                                                        \
		} else {                                                                         \
			nlohmann_json_j.get_to(nlohmann_json_t.member);                              \
		}                                                                                \
	}
