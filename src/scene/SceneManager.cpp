#include "SceneManager.h"

#include <algorithm> // std::swap
#include <cassert> // assert

namespace ptgn {

void SceneManager::SetActiveScene(const char* scene_key) {
	//GetInstance().SetActiveSceneImpl(math::Hash(scene_key));
}

bool SceneManager::HasScene(const char* scene_key) {
	/*auto& instance{ GetInstance() };
	return instance.GetSceneImpl(math::Hash(scene_key)) != std::end(instance.scenes_);*/
	return false;
}

void SceneManager::UnloadScene(const char* scene_key) {
	//GetInstance().UnloadSceneImpl(math::Hash(scene_key));
}

Camera& SceneManager::GetActiveCamera() {
	return GetActiveScene().camera;
}

void SceneManager::LoadSceneImpl(std::size_t scene_key, Scene* scene) {
	assert(GetSceneImpl(scene_key) == std::end(scenes_) &&
		   "Cannot load a scene which already exists in SceneManager");
	scene->id_ = scene_key;
	scenes_.emplace_back(scene);
}

std::vector<Scene*>::iterator SceneManager::GetSceneImpl(std::size_t scene_key) {
	for (auto it{ scenes_.begin() }; it != std::end(scenes_); ++it) {
		if ((*it)->id_ == scene_key) {
			return it;
		}
	}
	return std::end(scenes_);
}

void SceneManager::SetActiveSceneImpl(std::size_t scene_key) {
	auto scene_it{ GetSceneImpl(scene_key) };
	assert(scene_it != std::end(scenes_) &&
		   "Cannot set active scene which has not been loaded into SceneManager");
	auto active_scene_it{ scenes_.begin() };
	// Only update previous scene to the first inactivated scene in a frame.
	// This makes multiple SetActiveScene calls only cause the first scene
	// to be renderered again.
	if (previous_scene_ == nullptr && scenes_.size() > 1) {
		previous_scene_ = *active_scene_it;
	}
	// Swap the contents of the scene iterator to the front of the vector.
	std::swap(*scene_it, *active_scene_it);
}

void SceneManager::UnloadSceneImpl(std::size_t scene_key) {
	for (auto scene : scenes_) {
		if (scene->id_ == scene_key) {
			scene->destroy_ = true;
			return;
		}
	}
}

void SceneManager::RenderActiveScene() {
	/*auto& instance{ GetInstance() };
	auto& scenes{ instance.scenes_ };
	auto previous_scene{ instance.previous_scene_ };
	if (previous_scene != nullptr) {
		previous_scene->Render();
	} else {
		auto active_scene{ scenes.front() };
		assert(active_scene != nullptr &&
			   "Cannot update active scene if it has been deleted");
		active_scene->Render();
	}*/
}

Scene& SceneManager::GetActiveScene() {
	/*auto& instance{ GetInstance() };
	auto& scenes{ instance.scenes_ };
	auto previous_scene{ instance.previous_scene_ };
	if (previous_scene != nullptr) {
		return *previous_scene;
	} else {
		auto active_scene{ scenes.front() };
		assert(active_scene != nullptr &&
			   "Cannot retrieve active scene if it has been deleted");
		return *active_scene;
	}*/
	return Scene{};
}

void SceneManager::UnloadFlaggedScenes() {
	//auto& instance{ GetInstance() };
	//auto& scenes{ instance.scenes_ };
	//// Cannot unload active scene (first element).
	//if (scenes.size() > 1) {
	//	// Start from first non-active scene.
	//	auto it{ ++std::begin(scenes) };
	//	while (it != std::end(scenes)) {
	//		auto scene{ *it };
	//		assert(scene != nullptr);
	//		// If scene if flagged for destruction and is not previous scene (unloaded separately in UpdateActiveSceneImpl).
	//		if (scene->destroy_ && scene != instance.previous_scene_) {
	//			// Destroy scene and remove it from scene vector.
	//			delete scene;
	//			*it = nullptr;
	//			it = scenes.erase(it);
	//		} else {
	//			++it;
	//		}
	//	}
	//}
}

void SceneManager::UpdateActiveScene() {
	//auto& instance{ GetInstance() };
	//auto& scenes{ instance.scenes_ };
	//auto active_scene{ scenes.front() };
	//assert(active_scene != nullptr &&
	//	   "Cannot update active scene if it has been deleted");
	//bool previous_scene{ instance.previous_scene_ != nullptr };
	//// Check if there was a scene previously.
	//if (previous_scene) {
	//	// If there was, exit it.
	//	instance.previous_scene_->Exit();
	//	// If that scene happened to be marked for deletion, delete it now.
	//	if (instance.previous_scene_->destroy_) {
	//		// Delete it and clear its instance in the scenes_ vector.
	//		auto it{ instance.GetSceneImpl(instance.previous_scene_->id_) };
	//		delete instance.previous_scene_;
	//		*it = nullptr;
	//		instance.scenes_.erase(it);
	//	}
	//}
	//// If there was a previous scene or this is the first scene added to the scene manager.
	//if (previous_scene || instance.scenes_.size() == 1) {
	//	// Initialize scenes once.
	//	if (!active_scene->init_) {
	//		active_scene->Init();
	//		active_scene->init_ = true;
	//	}
	//	// Enter scene each time a previous scene was set.
	//	active_scene->Enter();
	//}
	//// Reset previous scene to nullptr after each update cycle.
	//instance.previous_scene_ = nullptr;
	//// Update active scene for this cycle.
	//active_scene->Update();
}

SceneManager::~SceneManager() {
	for (auto& scene : scenes_) {
		delete scene;
		scene = nullptr;
	}
}

} // namespace ptgn