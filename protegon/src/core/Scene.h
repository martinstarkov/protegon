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
	bool init_{ false };
};

} // namespace ptgn

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
	return ptgn::math::Ceil<int>(value * active_camera_->scale.x);
}
int ScaleY(double value) const {
	assert(active_camera_ != nullptr && "Scene camera has not been set");
	return ptgn::math::Ceil<int>(value * active_camera_->scale.y);
}
*/