#pragma once

#include <vector>

#include "core/math/geometry/origin.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

struct Depth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;
};

namespace impl {

struct EntityDepthCompare {
	EntityDepthCompare() = default;
	explicit EntityDepthCompare(bool ascending);

	bool operator()(Entity a, Entity b) const;

	bool ascending{ true };
};

void SetDraw(Entity entity, std::size_t drawable_type_hash);

struct CapsuleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct CircleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct EllipseDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct ArcDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct PolygonDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RoundedRectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct TriangleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct LineDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

} // namespace impl

void SortByDepth(std::vector<Entity>& entities, bool ascending = true);

void SetDrawOrigin(Entity entity, Origin origin);

Origin GetDrawOrigin(Entity entity);

template <DrawableType T>
void SetDraw(Entity entity) {
	impl::SetDraw(entity, Hash<T>());
}

[[nodiscard]] bool HasDraw(Entity entity);

void RemoveDraw(Entity entity);

void SetDepth(Entity entity, Depth depth);

Depth GetDepth(Entity entity);

void SetBlendMode(Entity entity, BlendMode blend_mode);

BlendMode GetBlendMode(Entity entity);

PTGN_REGISTER_DRAWABLE(impl::CapsuleDraw);
PTGN_REGISTER_DRAWABLE(impl::CircleDraw);
PTGN_REGISTER_DRAWABLE(impl::EllipseDraw);
PTGN_REGISTER_DRAWABLE(impl::ArcDraw);
PTGN_REGISTER_DRAWABLE(impl::PolygonDraw);
PTGN_REGISTER_DRAWABLE(impl::RectDraw);
PTGN_REGISTER_DRAWABLE(impl::RoundedRectDraw);
PTGN_REGISTER_DRAWABLE(impl::TriangleDraw);
PTGN_REGISTER_DRAWABLE(impl::LineDraw);

} // namespace ptgn