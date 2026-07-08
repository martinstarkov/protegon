#pragma once

#include <vector>

#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/shader.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

struct EffectMargin {
	/// @brief Number of pixels on all sides added to the effect render target.
	/// Only applies to texture effects.
	int value{ 0 };

	PTGN_SERIALIZE_VALUE(EffectMargin, value)
};

namespace impl {

struct IgnoreParentDepth {};

struct EffectTag {};

struct HDREffectTag {};

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

void SetFillStyle(Entity entity, FillStyle fill_style);

template <DrawableType T>
void SetDraw(Entity entity) {
	impl::DrawableRegistrar<T>::Touch();
	impl::SetDraw(entity, Hash<T>());
}

/// @return True if the entity has a drawable component with the specified drawable type hash.
template <DrawableType T>
bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>() && Hash<T>() == entity.Get<impl::IDrawable>().hash;
}

/// @return True if the entity has any drawable component.
[[nodiscard]] bool HasDraw(Entity entity);

void RemoveDraw(Entity entity);

void SetDepth(Entity entity, Depth depth);

Depth GetDepth(Entity entity);

void IgnoreParentDepth(Entity entity, bool ignore_parent_depth = true);

void SetBlendMode(Entity entity, BlendMode blend_mode);

BlendMode GetBlendMode(Entity entity);

} // namespace ptgn