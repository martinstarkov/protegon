#pragma once

#include <ecs/ecs.h>

#include <utility>

#include "app/application_context.h"
#include "core/assert.h"
#include "core/util/concepts.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
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
	effect.Add<T>(std::forward<TArgs>(args)...);
	effect.Add<EffectTag>();

	if constexpr (EffectTraits<T>::requires_hdr) {
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

void ClearEffects(Entity entity) {
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

void ClearEffects(const Scene& scene) {
	ClearEffects(scene.GetRenderTarget());
}

void ClearScreenEffects(Scene& scene) {
	impl::ApplicationAccessor::ctx(impl::SceneContextAccessor::app(scene.ctx()))
		.screen_effect_manager.Reset();
}

} // namespace ptgn