#pragma once

#include <compare>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/tolerance.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Depth {
	Depth() = default;

	Depth(float value) : value{ value } {} // NOSONAR

	[[nodiscard]] Depth RelativeTo(Depth parent) const;

	friend bool operator==(const Depth& lhs, const Depth& rhs) {
		return NearlyEqual(lhs.value, rhs.value);
	}

	friend std::partial_ordering operator<=>(const Depth& lhs, const Depth& rhs) {
		if (NearlyEqual(lhs.value, rhs.value)) {
			return std::partial_ordering::equivalent;
		}

		if (lhs.value < rhs.value) {
			return std::partial_ordering::less;
		}

		if (lhs.value > rhs.value) {
			return std::partial_ordering::greater;
		}

		return std::partial_ordering::unordered;
	}

	operator float() const { // NOSONAR
		return value;
	}

	float value{ 0.0f };

	PTGN_SERIALIZE_VALUE(Depth, value)
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