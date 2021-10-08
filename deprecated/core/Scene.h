#pragma once

#include "core/Camera.h"
#include "core/ECS.h"

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Init() {}
	virtual void Enter() {}
	virtual void Update() {}
	virtual void Render() {}
	virtual void Exit() {}
	ecs::Manager manager;
	Camera camera;
private:
	friend class SceneManager;

	// Flags for use in SceneManager.

	bool init_{ false };
	bool destroy_{ false };
	std::size_t id_{ 0 };
};

} // namespace ptgn