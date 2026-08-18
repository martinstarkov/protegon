#pragma once

#include <cstdint>
#include <vector>

#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_filter.h"
#include "runtime/physics/collider.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class GroundingDirection : std::uint8_t {
	AgainstGravity,
	Up,
	Down,
	Left,
	Right,
	Any,
};
PTGN_REFLECT_ENUM(GroundingDirection);

enum class GroundEntityScope : std::uint8_t {
	Collider,
	Root,
};
PTGN_REFLECT_ENUM(GroundEntityScope);

struct PlatformerGrounding {
	bool enabled{ true };
	GroundingDirection direction{ GroundingDirection::AgainstGravity };
	Degrees max_angle{ 45.0f };
	std::vector<ColliderMask> masks;
	EntityFilter entities;
	GroundEntityScope entity_scope{ GroundEntityScope::Root };

	PTGN_REFLECT(
		PlatformerGrounding, enabled, direction, max_angle, masks, entities, entity_scope
	)
};

struct PlatformerGroundingState {
	bool grounded{ false };
	bool previous_grounded{ false };
	Entity ground_entity;
	V2_float ground_normal;
};

[[nodiscard]] V2_float GetGroundingNormal(
	GroundingDirection direction, V2_float gravity
);

[[nodiscard]] bool MatchesGroundingNormal(
	V2_float normal, const PlatformerGrounding& grounding, V2_float gravity
);

void UpdatePlatformerGrounding(
	Entity entity, const PlatformerGrounding& grounding, PlatformerGroundingState& state,
	V2_float gravity
);

} // namespace ptgn
