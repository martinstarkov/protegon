#include "runtime/graphics/draw.h"

#include <algorithm>
#include <compare>
#include <vector>

#include "core/graphics/fill_style.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

namespace impl {

void SetDraw(Entity entity, std::size_t drawable_type_hash) {
	entity.Add<IDrawable>(drawable_type_hash);
}

EntityDepthCompare::EntityDepthCompare(bool compare_ascending) : ascending{ compare_ascending } {}

bool EntityDepthCompare::operator()(Entity a, Entity b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return ascending ? a.WasCreatedBefore(b) : b.WasCreatedBefore(a);
	}
	return ascending ? depth_a < depth_b : depth_a > depth_b;
}

} // namespace impl

bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>();
}

void RemoveDraw(Entity entity) {
	entity.Remove<impl::IDrawable>();
}

void SortByLocalDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, [ascending](Entity a, Entity b) {
		auto depth_a{ GetLocalDepth(a) };
		auto depth_b{ GetLocalDepth(b) };

		if (depth_a == depth_b) {
			return ascending ? a.WasCreatedBefore(b) : b.WasCreatedBefore(a);
		}

		return ascending ? depth_a < depth_b : depth_a > depth_b;
	});
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, impl::EntityDepthCompare{ ascending });
}

void SetFillStyle(Entity entity, FillStyle fill_style) {
	entity.Add<FillStyle>(fill_style);
}

void SetDepth(Entity entity, Depth depth) {
	entity.Add<Depth>(depth);
}

Depth GetLocalDepth(Entity entity) {
	return entity.GetOrDefault<Depth>();
}

Depth GetDepth(Entity entity) {
	Depth depth{ GetLocalDepth(entity) };

	ForEachParent(
		entity, [](Entity e) { return e.Has<impl::IgnoreParentDepth>(); },
		[&depth](Entity parent) {
			depth.value += GetLocalDepth(parent).value;
			return true;
		}
	);

	return depth;
}

void IgnoreParentDepth(Entity entity, bool ignore_parent_depth) {
	if (ignore_parent_depth) {
		entity.Add<impl::IgnoreParentDepth>();
	} else {
		entity.Remove<impl::IgnoreParentDepth>();
	}
}

void SetBlendMode(Entity entity, BlendMode blend_mode) {
	entity.Add<BlendMode>(blend_mode);
}

BlendMode GetBlendMode(Entity entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

} // namespace ptgn