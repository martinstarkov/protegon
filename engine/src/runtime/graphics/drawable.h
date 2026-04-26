#pragma once

#include <concepts>
#include <string_view>
#include <unordered_map>

#include "core/util/hash.h"
#include "serialization/serialize.h"

// The reason for this instead of a virtual Draw() function in the entity class is because when
// using entity looping functions, the manager constructs Entity objects, which disables
// polymorphism. Instead, I opt for automatically registering classes which inherit from
// Drawable<ClassName> into a static map which contains static Draw functions and can be queried
// by / the type name of the class.

namespace ptgn {

class DrawContext;

class Entity;

template <typename T>
concept DrawableType = requires(DrawContext& render_context, Entity entity) {
	{ T::Draw(render_context, entity) } -> std::same_as<void>;
};

namespace impl {

class IDrawable {
public:
	IDrawable() = default;

	IDrawable(std::size_t type_hash) : hash{ type_hash } {}

	using DrawFunc = void (*)(DrawContext&, Entity);

	static auto& data() {
		static std::unordered_map<std::size_t, DrawFunc> s;
		return s;
	}

	PTGN_SERIALIZE(IDrawable, hash)

	std::size_t hash{ 0 };
};

template <DrawableType T>
class DrawableRegistrar {
	friend Entity;

	friend T;

	static bool RegisterDrawFunction() {
		IDrawable::data()[Hash<T>()] = &T::Draw;
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

#define PTGN_REGISTER_DRAWABLE(Type) template class impl::DrawableRegistrar<Type>

} // namespace ptgn