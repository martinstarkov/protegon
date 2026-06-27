#pragma once

#include <string_view>
#include <tuple>

#include "core/util/macro_loop.h"

namespace ptgn::reflection {

template <typename T>
struct Member {
	std::string_view name;
	T& value;
};

template <typename T>
Member(std::string_view, T&) -> Member<T>;

} // namespace ptgn::reflection

#define PTGN_REFLECT_MEMBER(member, object)        \
	::ptgn::reflection::Member {                   \
		std::string_view{ #member }, object.member \
	}

#define PTGN_REFLECT_MEMBERS(Type, ...)                                                   \
	friend constexpr auto ReflectMembers(Type& value) {                                   \
		return std::tuple{ PTGN_MAP_LIST_DATA(PTGN_REFLECT_MEMBER, value, __VA_ARGS__) }; \
	}                                                                                     \
	friend constexpr auto ReflectMembers(const Type& value) {                             \
		return std::tuple{ PTGN_MAP_LIST_DATA(PTGN_REFLECT_MEMBER, value, __VA_ARGS__) }; \
	}

#define PTGN_REFLECT_VALUE(Type, member)                    \
	friend constexpr auto ReflectValue(Type& value) {       \
		return PTGN_REFLECT_MEMBER(member, value);          \
	}                                                       \
	friend constexpr auto ReflectValue(const Type& value) { \
		return PTGN_REFLECT_MEMBER(member, value);          \
	}

#define PTGN_REFLECT_EMPTY(Type)                        \
	friend constexpr auto ReflectMembers(Type&) {       \
		return std::tuple{};                            \
	}                                                   \
	friend constexpr auto ReflectMembers(const Type&) { \
		return std::tuple{};                            \
	}
