#include "runtime/graphics/fx/effects.h"

#include <functional>

#include "core/assert.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/visible.h"

namespace ptgn::impl {

impl::EffectParams GetEffectParams(Entity entity) {
	if (!entity.Has<Effects>()) {
		return {};
	}

	impl::EffectParams params;

	params.draw_callback = [entity](DrawContext& ctx) {
		if (auto material_update{ entity.TryGet<MaterialUpdate>() }) {
			material_update->update(entity);
		}

		const auto& effects{ entity.Get<Effects>().effects };

		for (const auto& effect : effects) {
			PTGN_ASSERT(!IsVisible(entity), "Effects attached to entities must be invisible");
			InvokeDrawable(ctx, effect);
		}
	};

	// TODO: Get margin from effect entities.
	params.margin = 0;

	return params;
}

} // namespace ptgn::impl
