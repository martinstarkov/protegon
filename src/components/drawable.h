#pragma once

#include <concepts>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "common/type_info.h"
#include "math/hash.h"
#include "serialization/serializable.h"

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

template <typename T>
concept DrawableType = requires(impl::RenderData& ctx, const Entity& entity) {
	{ T::Draw(ctx, entity) } -> std::same_as<void>;
};

namespace impl {

class IDrawable {
public:
	IDrawable() = default;

	IDrawable(std::string_view name) : hash{ Hash(name) } {}

	using DrawFunc = void (*)(impl::RenderData&, const Entity&);

	static auto& data() {
		static std::unordered_map<std::size_t, DrawFunc> s;
		return s;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(IDrawable, hash)

	std::size_t hash{ 0 };
};

template <DrawableType T>
class DrawableRegistrar {
	friend Entity;

	friend T;

	static bool RegisterDrawFunction() {
		constexpr auto name{ type_name<T>() };
		IDrawable::data()[Hash(name)] = &T::Draw;
		return true;
	}

	static bool registered_draw;

	DrawableRegistrar() {
		(void)registered_draw;
	}
};

template <DrawableType T>
bool DrawableRegistrar<T>::registered_draw = DrawableRegistrar<T>::RegisterDrawFunction();

} // namespace impl

#define PTGN_DRAWABLE_REGISTER(Type) template class impl::DrawableRegistrar<Type>

} // namespace ptgn