#pragma once

#include "ecs/ECS.h"

#include "Chunk.h"

#include <vector>

#include "renderer/Camera.h"

namespace engine {

class Scene {
public:
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Camera* GetCamera() const;
	void SetCamera(Camera& camera);
	ecs::Manager manager;
	ecs::Manager ui_manager;
	ecs::Manager event_manager;
	// TODO: Move chunks vector elsewhere.
	std::vector<Chunk*> chunks;
	std::vector<Chunk*> player_chunks;
private:
	Camera* active_camera_ = nullptr;
};

} // namespace engine