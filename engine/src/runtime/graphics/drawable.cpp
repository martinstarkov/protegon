#include "runtime/graphics/drawable.h"

#include <algorithm>
#include <functional>

#include "core/assert.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/effect_params.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"

namespace ptgn::impl {

void InvokeDrawable(DrawContext& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	auto draw_function{ impl::IDrawable::FindDrawFunction(drawable.hash) };

	if (!draw_function) {
		PTGN_WARN("Failed to identify drawable function for hash: ", drawable.hash);
		return;
	}

	draw_function(ctx, entity);
}

EffectParams GetEffectParams(const Entity& entity) {
	EffectParams params{ .margin = 0 };

	if (!HasChildren(entity)) {
		if (entity.Has<MaterialUpdate>()) {
			params.draw_callback = [entity](DrawContext&) {
				if (auto material_update{ entity.TryGet<MaterialUpdate>() }) {
					material_update->update(CustomShader{ entity });
				}
			};
		}
		return params;
	}

	const auto& children{ GetChildren(entity) };

	std::size_t effect_count{ 0 };

	for (const auto& child : children) {
		if (child.Has<EffectTag>()) {
			effect_count += 1;
		}
		if (auto margin{ child.TryGet<EffectMargin>() }) {
			params.margin = std::max(params.margin, margin->value);
		}
		if (child.Has<HDREffectTag>()) {
			params.hdr = true;
		}
	}

	if (!effect_count) {
		if (entity.Has<MaterialUpdate>()) {
			params.draw_callback = [entity](DrawContext&) {
				if (auto material_update{ entity.TryGet<MaterialUpdate>() }) {
					material_update->update(CustomShader{ entity });
				}
			};
		}
		return params;
	}

	params.draw_callback = [entity](DrawContext& ctx) {
		if (auto material_update{ entity.TryGet<MaterialUpdate>() }) {
			material_update->update(CustomShader{ entity });
		}

		if (!HasChildren(entity)) {
			return;
		}

		for (const auto& child : GetChildren(entity)) {
			if (child.Has<EffectTag>()) {
				PTGN_ASSERT(!IsVisible(child), "Effects attached to entities must be invisible");
				InvokeDrawable(ctx, child);
			}
		}
	};

	return params;
}

} // namespace ptgn::impl