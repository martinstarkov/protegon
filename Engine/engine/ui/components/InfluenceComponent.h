#pragma once

#include "ecs/ECS.h"

struct InfluenceComponent {
	InfluenceComponent(ecs::Manager& manager) : manager{ manager } {}
	ecs::Manager& manager;
};