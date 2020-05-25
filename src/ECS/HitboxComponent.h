#pragma once
#include "Components.h"
#include "SDL.h"
#include "../AABB.h"

class HitboxComponent : public AABBComponent {
public:
	HitboxComponent() : AABBComponent(AABB(), this) {}
	HitboxComponent(AABB rectangle) : AABBComponent(rectangle, this) {}
};