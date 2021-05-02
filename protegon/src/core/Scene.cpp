#include "Scene.h"

#include "core/SceneManager.h"

namespace engine {

Display Scene::GetDisplay() const {
	return Engine::GetDisplay(display_index_);
}

V2_int Scene::GetWindowSize() const {
	return GetDisplay().first.GetSize();
}

void Scene::SetDisplayIndex(std::size_t new_display_index) {
	display_index_ = new_display_index;
}

std::size_t Scene::GetDisplayIndex() const {
	return display_index_;
}

} // namespace engine