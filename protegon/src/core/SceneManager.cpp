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
	instance.queued_scene_ = scene;
}

void SceneManager::UnloadScene(const char* scene_key) {
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(scene_key) };
	auto scene{ instance.GetScene(key) };
	bool unloading_queued_scene{
		instance.queued_scene_ == scene
	};
	bool unloading_active_scene{
		instance.queued_scene_ == nullptr &&
		instance.active_scene_ == scene 
	};
	assert(unloading_active_scene == false &&
		   "Cannot unload currently active scene if a new scene has not been queued first");
	assert(unloading_queued_scene == false &&
		   "Cannot unload currently queued scene");
	instance.unload_scenes_.emplace(key);
}

void SceneManager::LoadSceneImpl(const char* scene_key, Scene* scene) {
	const auto key{ math::Hash(scene_key) };
	auto existing_scene{ GetScene(key) };
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
	bool in_scene{ active_scene_ != nullptr };
	if (queued_scene_ != nullptr) {
		if (in_scene) {
			active_scene_->Exit();
		}
		if (!queued_scene_->entered_) {
			queued_scene_->Enter();
			queued_scene_->entered_ = true;
		}
		// As queued_scene_ is not nullptr, changing active_scene_ maintains the in_scene condition as true.
		active_scene_ = queued_scene_;
	}
	queued_scene_ = nullptr;
	if (in_scene) {
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
		instance.active_scene_->Render();
	}
}

void SceneManager::UnloadQueuedScenes() {
	auto& instance{ GetInstance() };
	for (const auto scene_key : instance.unload_scenes_) {
		// Remove loaded scene.
		auto it{ instance.scenes_.find(scene_key) };
		if (it != instance.scenes_.end()) {
			auto scene{ it->second };
			if (instance.active_scene_ == scene) {
				instance.active_scene_ = nullptr;
			}
			if (instance.queued_scene_ == scene) {
				instance.queued_scene_ = nullptr;
			}
			delete scene;
			instance.scenes_.erase(it);
		}
	}
	instance.unload_scenes_.clear();
}

SceneManager::~SceneManager() {
	for (auto [key, scene] : scenes_) {
		delete scene;
	}
}

} // namespace ptgn