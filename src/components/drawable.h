#pragma once

#include <cstddef>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "common/type_info.h"
#include "math/hash.h"

// The reason for this instead of a virtual Draw() function in the entity class is because when
// using entity looping functions, the manager constructs Entity objects, which disables
// polymorphism. Instead, I opt for automatically registering classes which inherit from
// Drawable<ClassName> into a static map which contains static Draw functions and can be queried by
// the type name of the class.

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class Entity;

namespace tt {

namespace impl {

template <typename, typename = std::void_t<>>
struct has_static_draw : std::false_type {};

template <typename T>
struct has_static_draw<
	T, std::void_t<decltype(T::Draw(
		   std::declval<ptgn::impl::RenderData&>(), std::declval<const Entity&>()
	   ))>> :
	std::is_same<
		decltype(T::Draw(std::declval<ptgn::impl::RenderData&>(), std::declval<const Entity&>())),
		void> {};

} // namespace impl

// Trait to detect static void Draw(impl::RenderData&, const Entity&)
template <typename T>
inline constexpr bool has_static_draw_v = impl::has_static_draw<T>::value;

} // namespace tt

class IDrawable {
public:
	IDrawable() = default;

	IDrawable(std::string_view name) : hash{ Hash(name) } {}

	using DrawFunc = void (*)(impl::RenderData&, const Entity&);

	static auto& data() {
		static std::unordered_map<std::size_t, DrawFunc> s;
		return s;
	}

	template <typename T>
	class Registrar {
		friend Entity;

		friend T;

		static bool RegisterDrawFunction() {
			static_assert(
				tt::has_static_draw_v<T>,
				"Cannot register draw interface class without static void "
				"Draw(impl::RenderData&, const Entity&) function"
			);
			constexpr auto name{ type_name<T>() };
			IDrawable::data()[Hash(name)] = &T::Draw;
			return true;
		}

		static bool registered_draw;

		Registrar() {
			(void)registered_draw;
		}
	};

	std::size_t hash{ 0 };
};

template <typename T>
using Drawable = IDrawable::Registrar<T>;

template <typename T>
bool IDrawable::Registrar<T>::registered_draw = IDrawable::Registrar<T>::RegisterDrawFunction();

namespace tt {

namespace impl {

template <typename T>
struct is_drawable : std::false_type {};

template <typename T>
struct is_drawable<IDrawable::Registrar<T>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_drawable_v = impl::is_drawable<T>::value;

} // namespace tt

} // namespace ptgn