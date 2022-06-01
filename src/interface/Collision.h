#pragma once

#include <algorithm>
#include <tuple>

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/Rectangle.h"
#include "core/ECS.h"
#include "physics/Collider.h"
#include "physics/Shape.h"
#include "core/Transform.h"
#include "physics/RigidBody.h"

namespace ptgn {

namespace collision {

struct CollisionManifold {
    V2_double point;
    V2_double normal;
    double time = 0.0;
};

struct Collision {
    ecs::Entity entity;
    CollisionManifold manifold;
};

// Sort times in order of lowest time first, if times are equal prioritize diagonal collisions first.
inline void SortTimes(std::vector<Collision>& collisions) {
    std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
        if (a.manifold.time != b.manifold.time) {
            return a.manifold.time < b.manifold.time;
        } else {
            return a.manifold.normal.MagnitudeSquared() < b.manifold.normal.MagnitudeSquared();
        }
    });
}

// Returns an AABB which encompasses the initial position and the future position of a dynamic AABB.
inline std::pair<V2_double, V2_double> GetBroadphaseBox(const V2_double& velocity, const V2_double& position, const V2_double& size) {
    V2_double broadphase_position;
    V2_double broadphase_size;
    broadphase_position.x = velocity.x > 0.0 ? position.x : position.x + velocity.x;
    broadphase_position.y = velocity.y > 0.0 ? position.y : position.y + velocity.y;
    broadphase_size.x = velocity.x > 0.0 ? velocity.x + size.x : size.x - velocity.x;
    broadphase_size.y = velocity.y > 0.0 ? velocity.y + size.y : size.y - velocity.y;
    return { broadphase_position, broadphase_size };
}
// Determine if a point lies inside an AABB
inline bool PointVsAABB(const V2_double& point, const V2_double& position, const V2_double& size) {
    return (point.x >= position.x &&
            point.y >= position.y &&
            point.x < position.x + size.x &&
            point.y < position.y + size.y);
}
// Find the penetration of one AABB into another AABB
inline V2_double IntersectAABB(const V2_double& position1, const V2_double& size1, const V2_double& position2, const V2_double& size2) {
    V2_double penetration;
    double dx = position1.x - position2.x;
    auto center1 = size1 / 2.0;
    auto center2 = size2 / 2.0;
    double px = (center1.x + center2.x) - std::abs(dx);
    if (px <= 0.0) {
        return penetration;
    }
    double dy = position1.y - position2.y;
    double py = (center1.y + center2.y) - std::abs(dy);
    if (py <= 0.0) {
        return penetration;
    }
    if (px < py) {
        double sx = math::Sign(dx);
        penetration.x = px * sx;
    } else {
        double sy = math::Sign(dy);
        penetration.y = py * sy;
    }
    return penetration;
}
// Check if two AABBs overlap
inline bool AABBVsAABB(const V2_double& position1, const V2_double& size1, const V2_double& position2, const V2_double& size2) {
    // If any side of the aabb it outside the other aabb, there cannot be an overlap.
    if (position1.x + size1.x <= position2.x || position1.x >= position2.x + size2.x) return false;
    if (position1.y + size1.y <= position2.y || position1.y >= position2.y + size2.y) return false;
    return true;
}
// Check if a ray collides with an AABB.
inline bool RayVsAABB(const V2_double& ray_origin, const V2_double& ray_dir, const V2_double& position, const V2_double& size, CollisionManifold& out_collision) {

    // Initial condition: no collision normal.
    out_collision.normal = { 0.0, 0.0 };
    out_collision.point = { 0.0, 0.0 };

    // Cache division.
    auto inv_dir = 1.0 / ray_dir;

    // Calculate intersections with rectangle bounding axes.
    auto t_near = (position - ray_origin) * inv_dir;
    auto t_far = (position + size - ray_origin) * inv_dir;

    // Discard 0 / 0 divisions.
    if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
    if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

    // Sort axis collision times so t_near contains the shorter time.
    if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
    if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

    // Early rejection.
    if (t_near.x > t_far.y || t_near.y > t_far.x) return false;

    // Closest time will be the first contact.
    out_collision.time = std::max(t_near.x, t_near.y);

    // Furthest time is contact on opposite side of target.
    double t_hit_far = std::min(t_far.x, t_far.y);

    // Reject if furthest time is negative, meaning the object is travelling away from the target.
    if (t_hit_far < 0.0) {
        return false;
    }

    // Contact point of collision from parametric line equation.
    out_collision.point = ray_origin + out_collision.time * ray_dir;

    // Find which axis collides further along the movement time.
    if (t_near.x > t_near.y) { // X-axis.
        // Direction of movement.
        if (inv_dir.x < 0.0) {
            out_collision.normal = { 1.0, 0.0 };
        } else {
            out_collision.normal = { -1.0, 0.0 };
        }
    } else if (t_near.x < t_near.y) { // Y-axis.
        // Direction of movement.
        if (inv_dir.y < 0.0) {
            out_collision.normal = { 0.0, 1.0 };
        } else {
            out_collision.normal = { 0.0, -1.0 };
        }
    } else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
        // Diagonal collision, set normal to opposite of direction of movement.
        out_collision.normal = ray_dir.Identity().Opposite();
    }

    // Raycast collision occurred.
    return true;
}
// Determine the time at which a dynamic AABB would collide with a static AABB.
inline bool DynamicAABBVsAABB(double dt, const V2_double& velocity, const V2_double& dynamic_position, const V2_double& dynamic_size, const V2_double& static_position, const V2_double& static_size, CollisionManifold& collision) {

    // Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
    if (velocity.IsZero()) {
        return false;
    }

    // Expand static target by dynamic object dimensions so that only the center of the dynamic object needs to be considered.
    V2_double expanded_position{ static_position - dynamic_size / 2.0 };
    V2_double expanded_size{ static_size + dynamic_size };

    // Check if the velocity ray collides with the expanded target.
    if (RayVsAABB(dynamic_position + dynamic_size / 2, velocity * dt, expanded_position, expanded_size, collision)) {
        return collision.time >= 0.0 && collision.time < 1.0;
    } else {
        return false;
    }
}
// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
inline bool ResolveDynamicAABBVsAABB(double dt, const V2_double& relative_velocity, V2_double& dynamic_velocity, const V2_double& dynamic_position, const V2_double& dynamic_size, const V2_double& static_position, const V2_double& static_size, const CollisionManifold& collision) {
    CollisionManifold repeat_check;
    // Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocity for the nearest one may invalidate the previously thought collisions.
    if (DynamicAABBVsAABB(dt, relative_velocity, dynamic_position, dynamic_size, static_position, static_size, repeat_check)) {
        dynamic_velocity += collision.normal * math::Abs(dynamic_velocity) * (1.0 - collision.time);
        /*if (std::abs(velocity.x) < LOWEST_VELOCITY) {
            velocity.x = 0.0;
        }
        if (std::abs(velocity.y) < LOWEST_VELOCITY) {
            velocity.y = 0.0;
        }*/
        return true;
    }
    return false;
}


namespace internal {

using CollisionCallback = Manifold(*)(const component::Transform& A,
									  const component::Transform& B,
									  const component::Shape& a,
									  const component::Shape& b);

extern CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)];

Manifold StaticAABBvsAABB(const component::Transform& A,
						  const component::Transform& B,
						  const component::Shape& a,
						  const component::Shape& b);

Manifold StaticCirclevsCircle(const component::Transform& A,
							  const component::Transform& B,
							  const component::Shape& a,
							  const component::Shape& b);

Manifold StaticAABBvsCircle(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

Manifold StaticCirclevsAABB(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

} // namespace internal

Manifold StaticIntersection(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

void Clear(ecs::Manager& manager);

void Update(ecs::Manager& manager, double dt);

void Resolve(ecs::Manager& manager);

// Check if two AABBs overlap.
bool AABBvsAABB(const component::Transform& A,
				const component::Transform& B,
				const component::Shape& a,
				const component::Shape& b);

} // namespace collision

} // namespace ptgn

inline std::ostream& operator<<(std::ostream& os, const ptgn::collision::CollisionManifold& obj) {
    os << "Point: " << obj.point << ", Normal: " << obj.normal << ", Time: " << obj.time;
    return os;
}