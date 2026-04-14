#include "runtime/graphics/draw.h"

#include <algorithm>
#include <vector>

#include "core/assert.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_context.h"

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

template <ShapeType T>
void DrawShape(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto tint{ GetTint(entity) };
	auto fill_style{ entity.GetOrDefault<FillStyle>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.DrawShape(shape, draw_transform, tint, fill_style, draw_origin, depth, blend_mode);
}

void CapsuleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Capsule>(renderer, entity, camera);
}

void CircleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Circle>(renderer, entity, camera);
}

void EllipseDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Ellipse>(renderer, entity, camera);
}

void ArcDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Arc>(renderer, entity, camera);
}

void PolygonDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Polygon>(renderer, entity, camera);
}

void RectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Rect>(renderer, entity, camera);
}

void RoundedRectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<RoundedRect>(renderer, entity, camera);
}

void TriangleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Triangle>(renderer, entity, camera);
}

void LineDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Line>(renderer, entity, camera);
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
	parent.value_ += *this;
	return parent;
}

} // namespace ptgn