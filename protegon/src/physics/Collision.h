#pragma once

#include <ostream> // std::ostream

#include "ecs/ECS.h"

#include "math/Vector2.h"

#include "Shape.h"

struct Manifold;
struct Body;

struct CollisionManifold {
	CollisionManifold() = default;
	CollisionManifold(const V2_double& normal) : normal{ normal } {}
	V2_double point;
	V2_double normal;
	double time = 0.0;
	double depth = 0.0;
	friend std::ostream& operator<<(std::ostream& os, const CollisionManifold& obj) {
		os << "Point: " << obj.point << ", Normal: " << obj.normal << ", Time: " << obj.time << ", Depth: " << obj.depth;
		return os;
	}
};

struct Collision {
	Collision() = default;
	// For creating a collision where manifold is unimportant (static collisions).
	Collision(const ecs::Entity& entity) : entity{ entity } {}

	Collision(const ecs::Entity& entity, const CollisionManifold& manifold) : entity{ entity }, manifold{ manifold } {}
	ecs::Entity entity;
	CollisionManifold manifold;
};

typedef void (*CollisionCallback)(Manifold* m, Body* a, Body* b);

extern CollisionCallback Dispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)];

void CircleVsCircle(Manifold* m, Body* a, Body* b);
void CircleVsPolygon(Manifold* m, Body* a, Body* b);
void PolygonVsCircle(Manifold* m, Body* a, Body* b);
void PolygonVsPolygon(Manifold* m, Body* a, Body* b);