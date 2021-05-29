#pragma once

#include "ecs/ECS.h"

namespace engine {

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Enter() {}
	virtual void Update() {}
	virtual void Render() {}
	virtual void Exit() {}
	ecs::Manager manager;
private:
	friend class SceneManager;
	bool entered_{ false };
};

} // namespace engine

/*
// Convert coordinates from world reference frame to screen reference frame.
V2_int WorldToScreen(const V2_double& world_coordinate) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return Ceil((world_coordinate - active_camera_->offset) * active_camera_->scale);
}
// Convert coordinates from screen reference frame to world reference frame.
V2_int ScreenToWorld(const V2_double& screen_coordinate) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return Ceil(screen_coordinate / active_camera_->scale + active_camera_->offset);
}
V2_int Scale(const V2_double& size) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return Ceil(size * active_camera_->scale);
}
int ScaleX(double value) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return engine::math::Ceil<int>(value * active_camera_->scale.x);
}
int ScaleY(double value) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return engine::math::Ceil<int>(value * active_camera_->scale.y);
}
*/