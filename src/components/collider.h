#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn {

using CollisionCategory = std::int64_t;
using CollisionMask		= std::int64_t;
using CollisionCallback = std::function<void(ecs::Entity, ecs::Entity)>;

struct Collider {
	ecs::Entity parent;
	V2_float offset;
	Rect bounds;
	CollisionMask mask{ 0 };
	CollisionCategory category{ 0 };
	std::unordered_set<ecs::Entity> overlaps;
	CollisionCallback on_overlap_start;
	CollisionCallback on_overlap;
	CollisionCallback on_overlap_stop;
	bool enabled{ true };
	bool sweep{ false };
};

struct BoxCollider : public Collider {
	BoxCollider() = default;

	BoxCollider(
		ecs::Entity parent, const V2_float& size, Origin origin = Origin::Center,
		float rotation = 0.0f
	) :
		size{ size }, origin{ origin }, rotation{ rotation } {
		this->parent = parent;
	}

	// @return Rect in relative coordinates.
	[[nodiscard]] Rect GetRelativeRect() const {
		return { offset, size, origin, rotation };
	}

	// @return Rect in absolute coordinates (relative to its parent entity's transform). If the
	// parent entity has a Animation or Sprite component, this will be relative to the top left of
	// that (plus the the transform as before).
	[[nodiscard]] Rect GetAbsoluteRect() const;

	V2_float size;
	Origin origin{ Origin::Center };
	// Rotation in radians relative to the center of the box collider, also relative to the parent
	// entity transform rotation.
	float rotation{ 0.0f };
};

struct CircleCollider : public Collider {
	float radius{ 0.0f };
};

// struct PolygonCollider : public Collider {
//	std::vector<V2_float> vertices;
// };

// TODO: Add edge collider.

struct ColliderGroup {
	using Name = std::string;

	ecs::Entity parent;

	ecs::Manager group;

	std::unordered_map<Name, ecs::Entity> names;

	explicit ColliderGroup(ecs::Entity parent, const ecs::Manager& group) :
		parent{ parent }, group{ group } {}

	// @param offset Relative position of the box collider.
	// @param rotation Relative rotation of the box collider.
	// @param size Relative size of the box collider.
	// @param origin Origin of the box collider relative to its local position.
	// @param enabled Enable/disable collider by default.
	ecs::Entity AddBox(
		const Name& name, const V2_float& position, float rotation, const V2_float& size,
		Origin origin = Origin::Center, bool enabled = true,
		const CollisionCallback& on_overlap_start = nullptr,
		const CollisionCallback& on_overlap		  = nullptr,
		const CollisionCallback& on_overlap_stop  = nullptr
	) {
		auto entity			 = group.CreateEntity();
		auto& box			 = entity.Add<BoxCollider>(parent, size, origin, rotation);
		box.offset			 = position;
		box.enabled			 = enabled;
		box.on_overlap_start = on_overlap_start;
		box.on_overlap		 = on_overlap;
		box.on_overlap_stop	 = on_overlap_stop;
		names.emplace(name, entity);
		group.Refresh();
		return entity;
	}

	const BoxCollider& GetBox(const Name& name) const {
		auto e = Get(name);
		PTGN_ASSERT(e.Has<BoxCollider>());
		return e.Get<BoxCollider>();
	}

	ecs::Entity Get(const Name& name) const {
		auto it = names.find(name);
		PTGN_ASSERT(it != names.end(), "Failed to retrieve entity with given name");
		return it->second;
	}
};

} // namespace ptgn