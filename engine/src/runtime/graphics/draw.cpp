#include "runtime/graphics/draw.h"

#include <algorithm>
#include <compare>
#include <vector>

#include "core/math/geometry/origin.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

namespace impl {

void SetDraw(Entity entity, std::size_t drawable_type_hash) {
	entity.Add<IDrawable>(drawable_type_hash);
}

EntityDepthCompare::EntityDepthCompare(bool ascending) : ascending{ ascending } {}

bool EntityDepthCompare::operator()(Entity a, Entity b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return ascending ? a.WasCreatedBefore(b) : !a.WasCreatedBefore(b);
	}
	return ascending ? (depth_a < depth_b) : (depth_a > depth_b);
}

} // namespace impl

bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>();
}

void RemoveDraw(Entity entity) {
	entity.Remove<impl::IDrawable>();
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, impl::EntityDepthCompare{ ascending });
}

void SetDrawOrigin(Entity entity, Origin origin) {
	entity.Add<Origin>(origin);
}

Origin GetDrawOrigin(Entity entity) {
	return entity.GetOrDefault<Origin>(Origin::Center);
}

void SetDepth(Entity entity, Depth depth) {
	entity.Add<Depth>(depth);
}

Depth GetDepth(Entity entity) {
	// TODO: This was causing a bug with the mitosis disk background (rock texture) thing in GMTK
	// 2025. Figure out how to fix relative depths.
	/*Depth parent_depth{};
	if (HasParent(entity)) {
		auto parent{ GetParent(entity) };
		if (parent != entity && parent.Has<Depth>()) {
			parent_depth = GetDepth(parent);
		}
	}
	return parent_depth +*/
	return entity.GetOrDefault<Depth>();
}

void SetBlendMode(Entity entity, BlendMode blend_mode) {
	entity.Add<BlendMode>(blend_mode);
}

BlendMode GetBlendMode(Entity entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

Depth Depth::RelativeTo(Depth parent) const {
	parent.value += value;
	return parent;
}

} // namespace ptgn