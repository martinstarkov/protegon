#include "scene/scene.h"

namespace ptgn {

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
