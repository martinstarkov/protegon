#pragma once

#include <string_view>
#include <tuple>
#include <utility>

#include "core/util/macro_loop.h"

namespace ptgn::reflection {

template <typename T>
struct Member {
	std::string_view name;
	T& value;
};

template <typename T>
Member(std::string_view, T&) -> Member<T>;

template <typename T>
struct ReadOnlyMember {
	std::string_view name;
	const T& value;
};

template <typename T>
ReadOnlyMember(std::string_view, const T&) -> ReadOnlyMember<T>;

} // namespace ptgn::reflection

#define PTGN_IMPL_REFLECT_MEMBER(member, object)   \
	::ptgn::reflection::Member {                   \
		std::string_view{ #member }, object.member \
	}

#define PTGN_IMPL_REFLECT_READONLY_MEMBER(member, object) \
	::ptgn::reflection::ReadOnlyMember {                  \
		std::string_view{ #member }, object.member        \
	}

#define PTGN_IMPL_REFLECT_MEMBERS(Type, ...)                                              \
	friend constexpr auto ReflectMembers(Type& reflection_value_t) {                      \
		return std::tuple{                                                                \
			PTGN_MAP_LIST_DATA(PTGN_IMPL_REFLECT_MEMBER, reflection_value_t, __VA_ARGS__) \
		};                                                                                \
	}                                                                                     \
	friend constexpr auto ReflectMembers(const Type& reflection_value_t) {                \
		return std::tuple{                                                                \
			PTGN_MAP_LIST_DATA(PTGN_IMPL_REFLECT_MEMBER, reflection_value_t, __VA_ARGS__) \
		};                                                                                \
	}

#define PTGN_IMPL_REFLECT_DERIVED_MEMBERS(Type, Base, ...)                                      \
	friend constexpr auto ReflectMembers(Type& reflection_value_t) {                            \
		return std::tuple_cat(                                                                  \
			ReflectMembers(static_cast<Base&>(reflection_value_t)),                             \
			std::tuple{                                                                         \
				PTGN_MAP_LIST_DATA(PTGN_IMPL_REFLECT_MEMBER, reflection_value_t, __VA_ARGS__) } \
		);                                                                                      \
	}                                                                                           \
	friend constexpr auto ReflectMembers(const Type& reflection_value_t) {                      \
		return std::tuple_cat(                                                                  \
			ReflectMembers(static_cast<const Base&>(reflection_value_t)),                       \
			std::tuple{                                                                         \
				PTGN_MAP_LIST_DATA(PTGN_IMPL_REFLECT_MEMBER, reflection_value_t, __VA_ARGS__) } \
		);                                                                                      \
	}

#define PTGN_IMPL_REFLECT_READONLY_MEMBERS(Type, ...)                                              \
	friend constexpr auto ReflectReadOnlyMembers(Type& reflection_value_t) {                       \
		const Type& reflection_readonly_value_t{ reflection_value_t };                             \
		return std::tuple{ PTGN_MAP_LIST_DATA(                                                     \
			PTGN_IMPL_REFLECT_READONLY_MEMBER, reflection_readonly_value_t, __VA_ARGS__            \
		) };                                                                                       \
	}                                                                                              \
	friend constexpr auto ReflectReadOnlyMembers(const Type& reflection_value_t) {                 \
		return std::tuple{                                                                         \
			PTGN_MAP_LIST_DATA(PTGN_IMPL_REFLECT_READONLY_MEMBER, reflection_value_t, __VA_ARGS__) \
		};                                                                                         \
	}

#define PTGN_IMPL_REFLECT_VALUE(Type, member)                            \
	friend constexpr auto ReflectValue(Type& reflection_value_t) {       \
		return PTGN_IMPL_REFLECT_MEMBER(member, reflection_value_t);     \
	}                                                                    \
	friend constexpr auto ReflectValue(const Type& reflection_value_t) { \
		return PTGN_IMPL_REFLECT_MEMBER(member, reflection_value_t);     \
	}

#define PTGN_IMPL_REFLECT_EMPTY(Type)                   \
	friend constexpr auto ReflectMembers(Type&) {       \
		return std::tuple{};                            \
	}                                                   \
	friend constexpr auto ReflectMembers(const Type&) { \
		return std::tuple{};                            \
	}

/// @brief Adds inspector visible, read only reflection. These members are not included in the
/// automatically generated JSON representation from PTGN_REFLECT.
#define PTGN_REFLECT_READONLY(Type, ...) PTGN_IMPL_REFLECT_READONLY_MEMBERS(Type, __VA_ARGS__)
