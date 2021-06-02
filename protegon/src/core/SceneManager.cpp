#include "SceneManager.h"

namespace ptgn {

void SceneManager::SetActiveScene(const char* scene_key) {
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(scene_key) };
	auto it{ instance.scenes_.find(key) };
	bool scene_exists{ it != instance.scenes_.end() };
	assert(scene_exists &&
		   "Cannot enter scene which has not been loaded into SceneManager");
	auto scene{ it->second };
	assert(scene != nullptr &&
		   "Cannot set active scene to destroyed scene pointer");
	// Queue requested scene for next cycle.
	instance.queued_scene_ = scene;
}

void SceneManager::DestroyScene(const char* scene_key) {
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(scene_key) };
	auto scene{ instance.GetScene(key) };
	// Check if unloaded scene matches currently queued scene.
	bool destroying_queued_scene{
		instance.queued_scene_ == scene
	};
	// Check if unloaded scene matches currently active scene.
	bool destroying_active_scene{
		instance.queued_scene_ == nullptr &&
		instance.active_scene_ == scene 
	};
	assert(destroying_active_scene == false &&
		   "Cannot unload currently active scene if a new scene has not been queued first");
	assert(destroying_queued_scene == false &&
		   "Cannot unload currently queued scene");
	// Add scene key to set which is unloaded after the end of each frame.
	instance.destroy_scenes_.emplace(key);
}

bool SceneManager::HasScene(const char* scene_key) {
	return GetInstance().GetScene(math::Hash(scene_key)) != nullptr;
}

void SceneManager::AddSceneImpl(const char* scene_key, Scene* scene) {
	const auto key{ math::Hash(scene_key) };
	auto existing_scene{ GetScene(key) };
	// Check that scene key does not exist in the SceneManager already.
	assert(existing_scene == nullptr &&
		   "Cannot load scene with key which already exists in SceneManager");
	scenes_.emplace(key, scene);
}

Scene* SceneManager::GetScene(std::size_t key) {
	auto it{ scenes_.find(key) };
	Scene* scene{ nullptr };
	if (it != scenes_.end()) {
		scene = it->second;
	}
	return scene;
}

void SceneManager::UpdateActiveSceneImpl() {
	// Check if a scene is currently active.
	bool in_scene{ active_scene_ != nullptr };
	// Check if a scene was queued to be set during this cycle.
	if (queued_scene_ != nullptr) {
		if (in_scene) {
			// Exist previously active scene.
			active_scene_->Exit();
		}
		if (!queued_scene_->init_) {
			// Init the newly active scene once.
			queued_scene_->Init();
			queued_scene_->init_ = true;
		}
		// Enter the newly active scene each time a change occurs.
		queued_scene_->Enter();
		// As queued_scene_ is not nullptr, changing active_scene_ maintains the in_scene condition as true.
		active_scene_ = queued_scene_;
	}
	// Reset queued scene for following cycles.
	queued_scene_ = nullptr;
	if (in_scene) {
		// Update the currently active scene.
		active_scene_->Update();
	}
}

void SceneManager::UpdateActiveScene() {
	auto& instance{ GetInstance() };
	instance.UpdateActiveSceneImpl();
}

void SceneManager::RenderActiveScene() {
	auto& instance{ GetInstance() };
	if (instance.active_scene_ != nullptr) {
		// Render the currently active scene.
		instance.active_scene_->Render();
	}
}

void SceneManager::DestroyQueuedScenes() {
	auto& instance{ GetInstance() };
	for (const auto scene_key : instance.destroy_scenes_) {
		// Remove loaded scene.
		auto it{ instance.scenes_.find(scene_key) };
		if (it != instance.scenes_.end()) {
			auto scene{ it->second };
			// Check if the scene requested to be unloaded is the active or queued one.
			// Make sure to set them to nullptr if this is the case.
			if (instance.active_scene_ == scene) {
				instance.active_scene_ = nullptr;
			}
			if (instance.queued_scene_ == scene) {
				instance.queued_scene_ = nullptr;
			}
			// Destroy the flagged scene.
			delete scene;
			// Erase the flagged scene from the SceneManager.
			instance.scenes_.erase(it);
		}
	}
	// Clear flagged scene list.
	instance.destroy_scenes_.clear();
}

SceneManager::~SceneManager() {
	// Destroy all loaded scenes.
	for (auto [key, scene] : scenes_) {
		delete scene;
	}
}

} // namespace ptgn