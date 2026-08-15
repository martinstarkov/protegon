#pragma once

#include <ecs/ecs.h>

#include <string>
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
#include "runtime/graphics/fx/effect_registration.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
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

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> CreateEffect(Entity effect, TArgs&&... args) {
	EffectRegistry::Initialize<
		T,
		EffectRegistration<T>::Get().options.hdr
	>(effect, std::forward<TArgs>(args)...);
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
		impl::EffectRegistration<T>::Get().options.name.value_or(
			type_name_without_namespaces<T>()
		)
	};

	effect.Add<Tag>(name + " Entity");

	return impl::CreateEffect<T>(effect, std::forward<TArgs>(args)...);
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
EffectEntity<T> AddScreenEffect(Application& app, TArgs&&... args) {
	auto& app_context{ impl::ApplicationAccessor::ctx(app) };
	auto effect{ app_context.screen_effect_manager.CreateEntity() };
	auto result{ impl::CreateEffect<T>(effect, std::forward<TArgs>(args)...) };

	impl::RegisterScreenEffectEntity(
		app_context,
		result,
		impl::EffectRegistration<T>::Get().type_name
	);

	return result;
}

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
EffectEntity<T> AddScreenEffect(Scene& scene, TArgs&&... args) {
	return AddScreenEffect<T>(
		impl::SceneContextAccessor::app(scene.ctx()),
		std::forward<TArgs>(args)...
	);
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

inline void ClearScreenEffects(Application& app) {
	impl::ClearScreenEffects(impl::ApplicationAccessor::ctx(app));
}

inline void ClearScreenEffects(Scene& scene) {
	ClearScreenEffects(impl::SceneContextAccessor::app(scene.ctx()));
}

} // namespace ptgn
