#include "scene/scene.h"

namespace ptgn {
void Scene::PreUpdate() {}

void Scene::PostUpdate() {}

void Scene::Add(Action new_action) {
	actions_.insert(new_action);
}

void Scene::Remove(Action action) {
	actions_.erase(action);
}

bool Scene::HasActions() const {
	return !actions_.empty();
}

} // namespace ptgn
