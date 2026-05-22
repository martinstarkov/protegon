#pragma once

#include <concepts>
#include <utility>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

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

struct Effects {
	std::vector<Entity> effects;
};

} // namespace impl

template <typename T>
void AddEffect(Entity entity, EffectEntity<T> effect) {
	Hide(effect, false);
	entity.TryAdd<impl::Effects>().effects.emplace_back(effect);
}

template <typename T>
void AddEffect(const Scene& scene, EffectEntity<T> effect) {
	AddEffect(scene.GetRenderTarget(), effect);
}

template <typename T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
EffectEntity<T> CreateEffect(Scene& scene, TArgs&&... args) {
	auto effect{ scene.CreateEntity() };

	effect.Add<T>(std::forward<TArgs>(args)...);
	SetDraw<T>(effect);
	Show(effect, false);

	return EffectEntity<T>{ effect };
}

template <typename T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
EffectEntity<T> AddEffect(Entity entity, TArgs&&... args) {
	auto effect{ CreateEffect<T>(entity.GetScene(), std::forward<TArgs>(args)...) };
	AddEffect(entity, effect);
	return effect;
}

template <typename T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
EffectEntity<T> AddScreenEffect(Scene& scene, TArgs&&... args) {
	// TODO: Fix.
	auto effect{ CreateEffect<T>(scene, std::forward<TArgs>(args)...) };
	AddEffect(scene, effect);
	return effect;
}

} // namespace ptgn