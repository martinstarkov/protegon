#include "Scene.h"

#include "ecs/Components.h"

namespace engine {

Scene* Scene::instance_{ nullptr };

const std::shared_ptr<Camera> Scene::GetCamera() const {
	return active_camera_;
}

void Scene::SetCamera(Camera& camera) {
	active_camera_ = std::make_shared<Camera>(camera);
}

} // namespace engine