#pragma once

#include <engine/Include.h>

#include "Factories.h"

static void CreateWorld(ecs::Manager& manager, engine::Scene& scene) {
	auto player = CreateHopper(V2_double{ 1000/2.0, 600/2.0 }, manager, scene);
	//auto bottom_box = CreateBox(V2_double{ -10000000, 600 }, V2_double{ 1000000000, 1000000000 }, manager);
	/*
	auto top_box = CreateBox(V2_double{ 0,0 }, V2_double{ 1000, 32 }, manager);
	auto left_box = CreateBox(V2_double{ 0,32 }, V2_double{ 32, 600 - 32 - 32 }, manager);
	auto right_box = CreateBox(V2_double{ 1000-32, 32 }, V2_double{ 32, 600 - 32 - 32 }, manager);
	*/
	manager.Refresh();
}