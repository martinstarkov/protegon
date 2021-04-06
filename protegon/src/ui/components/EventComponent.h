#pragma once

#include "core/Scene.h"

struct EventComponent {
	EventComponent() = delete;
	EventComponent(engine::Scene& scene) : scene{ scene } {}
	engine::Scene& scene;
};