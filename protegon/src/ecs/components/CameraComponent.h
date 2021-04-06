#pragma once

#include "Component.h"

#include "renderer/Camera.h"

#include "core/Engine.h"

struct CameraComponent {
	CameraComponent(const engine::Camera& camera, bool primary) : camera{ camera }, primary{ primary } {}
	engine::Camera camera;
	bool primary = false;
};