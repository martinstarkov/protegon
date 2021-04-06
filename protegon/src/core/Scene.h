#pragma once

#include "ecs/ECS.h"

#include <vector> // std::vector
#include <memory> // std::shared_ptr

#include "core/World.h"
#include "renderer/Camera.h"

namespace engine {

class Scene {
public:
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene(Scene&&) = delete;

	static Scene& Get() {
		if (instance_ == nullptr) {
			instance_ = new Scene();
		}
		assert(instance_ != nullptr);
		return *instance_;
	}

	Camera* GetCamera();
	void SetCamera(Camera& camera);
	ecs::Manager ui_manager;
	ecs::Manager event_manager;
	BaseWorld* world{ nullptr };
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
private:
	static Scene* instance_;
	Camera* active_camera_{ nullptr };
};

} // namespace engine