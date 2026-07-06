#pragma once

#include <algorithm>
#include <concepts>
#include <string_view>
#include <vector>

#include "core/util/hash.h"
#include "core/util/macro.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/resources/shader.h"
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

template <typename T>
struct DrawableName {
	static constexpr std::string_view Get() {
		return "Unknown Drawable";
	}
};

class IDrawable {
public:
	IDrawable() = default;

	explicit IDrawable(std::size_t type_hash) : hash{ type_hash } {}

	using DrawFunc = void (*)(DrawContext&, Entity);

	struct Info {
		std::size_t hash{ 0 };
		std::string_view name;
		DrawFunc draw{ nullptr };
	};

	static auto& data() {
		static std::vector<Info> s;
		return s;
	}

	static bool Register(std::size_t type_hash, std::string_view name, DrawFunc draw) {
		auto& drawables{ data() };

		auto it{ std::ranges::find(drawables, type_hash, &Info::hash) };

		if (it != drawables.end()) {
			PTGN_ASSERT(
				it->draw == draw,
				"Drawable hash collision or duplicate drawable hash with different draw function"
			);

			return true;
		}

		drawables.push_back(
			Info{
				.hash = type_hash,
				.name = name,
				.draw = draw,
			}
		);

		std::ranges::sort(drawables, {}, &Info::name);

		return true;
	}

	static const Info* FindInfo(std::size_t type_hash) {
		auto& drawables{ data() };

		auto it{ std::ranges::find(drawables, type_hash, &Info::hash) };

		if (it == drawables.end()) {
			return nullptr;
		}

		return &*it;
	}

	static DrawFunc FindDrawFunction(std::size_t type_hash) {
		auto* info{ FindInfo(type_hash) };

		if (!info) {
			return nullptr;
		}

		return info->draw;
	}

	PTGN_SERIALIZE(IDrawable, hash)

	std::size_t hash{ 0 };
};

template <DrawableType T>
class DrawableRegistrar {
	friend Entity;

	friend T;

public:
	static void Touch() {
		(void)registered_draw;
	}

private:
	static bool RegisterDrawFunction() {
		return IDrawable::Register(Hash<T>(), DrawableName<T>::Get(), &T::Draw);
	}

	static bool registered_draw;

	DrawableRegistrar() {
		Touch();
	}
};

template <DrawableType T>
bool DrawableRegistrar<T>::registered_draw = DrawableRegistrar<T>::RegisterDrawFunction();

void InvokeDrawable(DrawContext& ctx, const Entity& entity);

EffectParams GetEffectParams(const Entity& entity);

} // namespace impl

} // namespace ptgn

#define PTGN_REGISTER_DRAWABLE_NAMED(Type, Name)  \
	template <>                                   \
	struct ::ptgn::impl::DrawableName<Type> {     \
		static constexpr std::string_view Get() { \
			return Name;                          \
		}                                         \
	};                                            \
	template class ::ptgn::impl::DrawableRegistrar<Type>

#define PTGN_REGISTER_DRAWABLE(Type) PTGN_REGISTER_DRAWABLE_NAMED(Type, #Type)

/// @param Type The effect type to register.
/// @param ... Optional bool value indicating whether the effect requires HDR rendering. Defaults to
/// false if not provided.
#define PTGN_REGISTER_EFFECT(Type, ...)                                                       \
	PTGN_REGISTER_DRAWABLE(Type);                                                             \
	template <>                                                                               \
	struct ::ptgn::impl::EffectTraits<Type> {                                                 \
		static constexpr bool requires_hdr{ PTGN_IMPL_FIRST_OR_DEFAULT(false, __VA_ARGS__) }; \
		static constexpr ::ptgn::impl::ColorRange color_range{                                \
			requires_hdr ? ::ptgn::impl::ColorRange::HDR : ::ptgn::impl::ColorRange::SDR      \
		};                                                                                    \
	}

/// @param Type The effect type to register.
/// @param ... Optional bool value indicating whether the effect requires HDR rendering. Defaults to
/// false if not provided.
#define PTGN_REGISTER_EFFECT_NAMED(Type, Name, ...)                                           \
	PTGN_REGISTER_DRAWABLE_NAMED(Type, Name);                                                 \
	template <>                                                                               \
	struct ::ptgn::impl::EffectTraits<Type> {                                                 \
		static constexpr bool requires_hdr{ PTGN_IMPL_FIRST_OR_DEFAULT(false, __VA_ARGS__) }; \
		static constexpr ::ptgn::impl::ColorRange color_range{                                \
			requires_hdr ? ::ptgn::impl::ColorRange::HDR : ::ptgn::impl::ColorRange::SDR      \
		};                                                                                    \
	}