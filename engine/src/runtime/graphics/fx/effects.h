#pragma once

#include <ecs/ecs.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "app/application_context.h"
#include "core/assert.h"
#include "core/util/concepts.h"
#include "core/util/type_info.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

template <typename T>
class EffectEntity : public Entity {
public:
	EffectEntity() = default;

	explicit EffectEntity(Entity entity) : Entity{ entity } {}

	static void Draw(DrawContext& ctx, Entity entity) {
		T::Draw(ctx, entity);
	}
};

namespace impl {

struct EffectRegistrationOptions {
	std::optional<std::string_view> name;
	bool hdr{ false };
};

struct EffectRegistrationData {
	std::string_view type_name;
	EffectRegistrationOptions options;
};

constexpr EffectRegistrationData MakeEffectRegistration(
	std::string_view type_name, EffectRegistrationOptions options = {}
) {
	return {
		.type_name = type_name,
		.options   = options,
	};
}

template <typename T>
struct EffectRegistration {
	static constexpr EffectRegistrationData Get() {
		return {};
	}
};

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> CreateEffect(Entity effect, TArgs&&... args) {
	effect.Add<T>(std::forward<TArgs>(args)...);
	effect.Add<EffectTag>();

	if constexpr (EffectRegistration<T>::Get().options.hdr) {
		effect.Add<HDREffectTag>();
	}

	SetDraw<T>(effect);
	effect.Add<Visible>(true);

	return EffectEntity<T>{ effect };
}

} // namespace impl

template <typename T>
void AddEffect(Entity entity, EffectEntity<T> effect) {
	effect.template Add<Visible>(false);
	PTGN_ASSERT(effect.template Has<impl::EffectTag>());
	AddChild(entity, effect);
}

template <typename T>
void AddEffect(const Scene& scene, EffectEntity<T> effect) {
	AddEffect(scene.GetRenderTarget(), effect);
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> CreateEffect(Scene& scene, TArgs&&... args) {
	auto effect{ scene.CreateEntity() };
	std::string name{
		impl::EffectRegistration<T>::Get().options.name.value_or(type_name_without_namespaces<T>())
	};
	effect.Add<Tag>(name + " Entity");
	impl::CreateEffect<T>(effect, std::forward<TArgs>(args)...);
	return EffectEntity<T>{ effect };
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> AddEffect(Entity entity, TArgs&&... args) {
	auto effect{ CreateEffect<T>(entity.GetScene(), std::forward<TArgs>(args)...) };
	AddEffect(entity, effect);
	return effect;
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> AddEffect(Scene& scene, TArgs&&... args) {
	auto effect{ CreateEffect<T>(scene, std::forward<TArgs>(args)...) };
	AddEffect(scene, effect);
	return effect;
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> AddScreenEffect(Scene& scene, TArgs&&... args) {
	auto effect{ impl::ApplicationAccessor::ctx(impl::SceneContextAccessor::app(scene.ctx()))
					 .screen_effect_manager.CreateEntity() };
	impl::CreateEffect<T>(effect, std::forward<TArgs>(args)...);
	return EffectEntity<T>{ effect };
}

template <typename T>
EffectEntity<T> AddEffectMargin(EffectEntity<T> effect, int margin) {
	effect.template Add<EffectMargin>(margin);
	return effect;
}

inline void ClearEffects(Entity entity) {
	if (!HasChildren(entity)) {
		return;
	}

	const auto& children{ GetChildren(entity) };
	for (Entity child : children) {
		if (child.Has<impl::EffectTag>()) {
			child.Destroy();
		}
	}
}

inline void ClearEffects(const Scene& scene) {
	ClearEffects(scene.GetRenderTarget());
}

inline void ClearScreenEffects(Scene& scene) {
	impl::ApplicationAccessor::ctx(impl::SceneContextAccessor::app(scene.ctx()))
		.screen_effect_manager.Reset();
}

} // namespace ptgn

#define PTGN_REGISTER_EFFECT(Type, ...)                                                   \
	template <>                                                                           \
	struct ::ptgn::impl::EffectRegistration<Type> {                                       \
		static constexpr ::ptgn::impl::EffectRegistrationData Get() {                     \
			return ::ptgn::impl::MakeEffectRegistration(                                  \
				#Type __VA_OPT__(, ::ptgn::impl::EffectRegistrationOptions __VA_ARGS__)   \
			);                                                                            \
		}                                                                                 \
	};                                                                                    \
	template <>                                                                           \
	struct ::ptgn::impl::DrawableRegistration<Type> {                                     \
		static constexpr ::ptgn::impl::DrawableRegistrationData Get() {                   \
			constexpr auto registration{ ::ptgn::impl::EffectRegistration<Type>::Get() }; \
                                                                                          \
			return ::ptgn::impl::MakeDrawableRegistration(                                \
				registration.type_name, {                                                 \
											.name  = registration.options.name,           \
											.group = "Effects",                           \
										}                                                 \
			);                                                                            \
		}                                                                                 \
	};                                                                                    \
	template class ::ptgn::impl::DrawableRegistrar<Type>