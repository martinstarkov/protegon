#pragma once

#include <string>
#include <type_traits>

#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

class DrawContext;

class Effect : public Entity {
public:
	Effect() = default;

	explicit Effect(Entity entity) : Entity{ entity } {}

	static void Draw(DrawContext& draw, Entity entity);
};

PTGN_REGISTER_DRAWABLE(Effect);

template <typename TEffect, typename... TArgs>
	requires std::is_constructible_v<TEffect, TArgs...>
Effect CreateEffect(Scene& scene, TArgs&&... args) {
	Effect effect{ scene.CreateEntity() };

	effect.Add<TEffect>(std::forward<TArgs>(args)...);
	effect.Add<impl::EffectName>(std::string{ impl::EffectNameOf<TEffect>::value });

	SetDraw<Effect>(effect);
	Show(effect, false);

	return effect;
}

} // namespace ptgn