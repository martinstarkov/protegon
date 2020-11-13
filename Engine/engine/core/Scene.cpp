#include "Scene.h"

#include "ecs/Components.h"

namespace engine {

Camera& Scene::GetCamera() const {
	assert(active_camera_ != nullptr && "Scene must contain camera");
	return *active_camera_;
}

void Scene::SetCamera(Camera& camera) {
	active_camera_ = &camera;
}

} // namespace engine