#pragma once
#include "Components.h"
#include "SDL.h"
#include "../AABB.h"

class HitboxComponent : public AABBComponent {
public:
	HitboxComponent(AABB rectangle) : AABBComponent(rectangle) {}
};