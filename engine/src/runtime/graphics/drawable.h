#pragma once

#include <algorithm>
#include <concepts>
#include <optional>
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

struct DrawableRegistrationOptions {
	std::optional<std::string_view> name;
	std::optional<std::string_view> group;
};

struct DrawableRegistrationData {
	std::string_view type_name;
	DrawableRegistrationOptions options;
};

constexpr DrawableRegistrationData MakeDrawableRegistration(
	std::string_view type_name, DrawableRegistrationOptions options = {}
) {
	return {
		.type_name = type_name,
		.options   = options,
	};
}

template <typename T>
struct DrawableRegistration {
	static constexpr DrawableRegistrationData Get() {
		return {};
	}
};

class IDrawable {
public:
	IDrawable() = default;

	explicit IDrawable(std::size_t type_hash) : hash{ type_hash } {}

	using DrawFunc = void (*)(DrawContext&, Entity);

	struct Info {
		std::size_t hash{ 0 };

		/// @brief Actual registered C++ type name.
		std::string_view type_name;

		/// @brief Optional name.
		std::optional<std::string_view> name;

		/// @brief Optional group.
		std::optional<std::string_view> group;

		DrawFunc draw{ nullptr };

		[[nodiscard]] constexpr std::string_view GetDisplayName() const {
			return name.value_or(type_name);
		}
	};

	static auto& data() {
		static std::vector<Info> s;
		return s;
	}

	static bool Register(
		std::size_t type_hash, const DrawableRegistrationData& registration, DrawFunc draw
	) {
		auto& drawables{ data() };

		auto it{ std::ranges::find(drawables, type_hash, &Info::hash) };

		if (it != drawables.end()) {
			PTGN_ASSERT(
				it->draw == draw && it->type_name == registration.type_name &&
					it->name == registration.options.name &&
					it->group == registration.options.group,
				"Drawable hash collision or duplicate drawable registration with different metadata"
			);

			return true;
		}

		drawables.push_back(
			Info{
				.hash	   = type_hash,
				.type_name = registration.type_name,
				.name	   = registration.options.name,
				.group	   = registration.options.group,
				.draw	   = draw,
			}
		);

		std::ranges::sort(drawables, {}, [](const Info& info) { return info.GetDisplayName(); });

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

	PTGN_REFLECT(IDrawable, hash)

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
		return IDrawable::Register(Hash<T>(), DrawableRegistration<T>::Get(), &T::Draw);
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

#define PTGN_REGISTER_DRAWABLE(Type, ...)                                                 \
	template <>                                                                           \
	struct ::ptgn::impl::DrawableRegistration<Type> {                                     \
		static constexpr ::ptgn::impl::DrawableRegistrationData Get() {                   \
			return ::ptgn::impl::MakeDrawableRegistration(                                \
				#Type __VA_OPT__(, ::ptgn::impl::DrawableRegistrationOptions __VA_ARGS__) \
			);                                                                            \
		}                                                                                 \
	};                                                                                    \
	template class ::ptgn::impl::DrawableRegistrar<Type>