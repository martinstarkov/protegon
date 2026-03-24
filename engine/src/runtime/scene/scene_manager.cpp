#include "runtime/scene/scene_manager.h"

namespace ptgn {

void SceneManager::Update() {
	// TODO: Fix.
}

LocalSceneManager::LocalSceneManager(SceneManager& scene_manager) :
	scene_manager_{ scene_manager } {}

} // namespace ptgn