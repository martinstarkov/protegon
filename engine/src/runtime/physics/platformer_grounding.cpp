#include "runtime/physics/platformer_grounding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

#include "runtime/ecs/entity_filter.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision.h"
#include "runtime/scene/scene.h"

namespace ptgn {

V2_float GetGroundingNormal(GroundingDirection direction, V2_float gravity) {
	switch (direction) {
		case GroundingDirection::AgainstGravity:
			return gravity.IsZero() ? V2_float{} : -gravity.Normalized();
		case GroundingDirection::Up: return { 0.0f, -1.0f };
		case GroundingDirection::Down: return { 0.0f, 1.0f };
		case GroundingDirection::Left: return { -1.0f, 0.0f };
		case GroundingDirection::Right: return { 1.0f, 0.0f };
		case GroundingDirection::Any: return {};
	}

	return {};
}

bool MatchesGroundingNormal(
	V2_float normal, const PlatformerGrounding& grounding, V2_float gravity
) {
	if (normal.IsZero()) {
		return false;
	}

	if (grounding.direction == GroundingDirection::Any) {
		return true;
	}

	V2_float expected{ GetGroundingNormal(grounding.direction, gravity) };

	if (expected.IsZero()) {
		return false;
	}

	float angle{ std::clamp(grounding.max_angle.value, 0.0f, 180.0f) };
	float min_dot{ std::cos(Degrees{ angle }.ToRad().value) };

	return normal.Normalized().Dot(expected.Normalized()) >= min_dot;
}

void UpdatePlatformerGrounding(
	Entity entity, const PlatformerGrounding& grounding, PlatformerGroundingState& state,
	V2_float gravity
) {
	state.previous_grounded = state.grounded;
	state.grounded = false;
	state.ground_entity = {};
	state.ground_normal = {};

	if (!grounding.enabled || !entity || !entity.Has<Collider>()) {
		return;
	}

	const auto& collider{ entity.Get<Collider>() };
	float best_score{ -std::numeric_limits<float>::infinity() };
	V2_float expected{ GetGroundingNormal(grounding.direction, gravity) };

	auto consider = [&](const CollisionInfo& collision) {
		if (!collision.entity || !MatchesGroundingNormal(collision.normal, grounding, gravity)) {
			return;
		}

		if (!grounding.masks.empty()) {
			const auto other_collider{ collision.entity.TryGet<Collider>() };
			if (!other_collider ||
				!std::ranges::contains(grounding.masks, other_collider->GetMask())) {
				return;
			}
		}

		Entity target{ collision.entity };
		if (grounding.entity_scope == GroundEntityScope::Root) {
			Entity root{ GetRootEntity(target) };
			if (root) {
				target = root;
			}
		}

		Scene& scene{ entity.GetScene() };
		if (!Matches(grounding.entities, scene, entity, target)) {
			return;
		}

		float score{
			grounding.direction == GroundingDirection::Any || expected.IsZero()
				? 1.0f
				: collision.normal.Normalized().Dot(expected.Normalized())
		};

		if (score <= best_score) {
			return;
		}

		best_score = score;
		state.grounded = true;
		state.ground_entity = target;
		state.ground_normal = collision.normal;
	};

	for (const auto& collision : collider.GetIntersections()) {
		consider(collision);
	}

	for (const auto& collision : collider.GetSweeps()) {
		consider(collision);
	}
}

} // namespace ptgn
