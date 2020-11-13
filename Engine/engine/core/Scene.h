#pragma once

#include "ecs/ECS.h"

#include "renderer/Camera.h"

namespace engine {

class Scene {
public:
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Camera& GetCamera() const;
	void SetCamera(Camera& camera);
	ecs::Manager manager;
	ecs::Manager ui_manager;
	ecs::Manager event_manager;
private:
	Camera* active_camera_ = nullptr;
};

} // namespace engine