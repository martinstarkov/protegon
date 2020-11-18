#include "Scene.h"

#include "ecs/Components.h"

namespace engine {

Camera* Scene::GetCamera() const {
	return active_camera_;
}

void Scene::SetCamera(Camera& camera) {
	active_camera_ = &camera;
}

} // namespace engine