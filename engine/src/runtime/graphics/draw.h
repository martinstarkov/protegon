#pragma once

#include <vector>

#include "core/math/geometry/origin.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

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

} // namespace ptgn