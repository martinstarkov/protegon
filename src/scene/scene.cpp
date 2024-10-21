#include "scene/scene.h"

namespace ptgn {

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
}

} // namespace ptgn