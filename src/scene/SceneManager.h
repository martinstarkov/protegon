#pragma once

#include <vector>
#include <memory>
#include <cassert>

#include "manager/ResourceManager.h"
#include "scene/Scene.h"
#include "math/Hash.h"

namespace ptgn {

class SceneManager : public manager::ResourceManager<Scene>{
public: 
	template <typename ...TArgs,
		std::enable_if_t<std::is_constructible_v<Scene, TArgs...>, bool> = true>
	Scene& Load(const std::size_t scene_key, TArgs&&... constructor_args) {
		auto& scene{ manager::ResourceManager<Scene>::Load(scene_key, std::forward<TArgs>(constructor_args)...) };
		scene.id_ = scene_key;
		return scene;
	}

	void Unload(const std::size_t scene_key) {
		assert(Has(scene_key) && "Cannot unload a scene which has not been loaded into the scene manager");
		RemoveActiveScene(scene_key);
		FlagScene(scene_key);
	}

	void AddActiveScene(const std::size_t scene_key) {
		assert(Has(scene_key) && "Cannot add active scene which has not been loaded into the scene manager");
		auto scene{ Get(scene_key) };
		scene->Enter();
		active_scenes_.emplace_back(scene);
	}

	void RemoveActiveScene(const std::size_t scene_key) {
		assert(Has(scene_key) && "Cannot remove active scene which has not been loaded into the scene manager");
		for (auto it{ active_scenes_.begin() }; it != active_scenes_.end();) {
			auto& scene{ *it };
			if (scene->id_ == scene_key)
				active_scenes_.erase(it++);
			else
				++it;
		}
	}

	std::shared_ptr<Scene> GetActiveScene() {
		if (active_scenes_.size() > 0)
			return active_scenes_.back();
		return nullptr;
	}

	void Update() {
		auto active_scene{ GetActiveScene() };
		if (active_scene != nullptr)
			active_scene->Update();
		if (flagged_scenes_ > 0)
			UnloadFlaggedScenes();
	}
private:
	void FlagScene(const std::size_t scene_key) {
		assert(Has(scene_key) && "Cannot flag a scene which has not been loaded into the scene manager");
		auto scene{ Get(scene_key) };
		scene->destroy_ = true;
		flagged_scenes_++;
	}

	void UnloadFlaggedScenes() {
		auto& map{ GetMap() };
		for (auto it{ map.begin() }; it != map.end() && flagged_scenes_ > 0;) {
			auto& scene{ it->second };
			if (scene->destroy_) {
				map.erase(it++);
				flagged_scenes_--;
			} else {
				++it;
			}
		}
		assert(flagged_scenes_ == 0 && "Failed to delete a flagged scene");
	}

	std::size_t flagged_scenes_{ 0 };
	std::vector<std::shared_ptr<Scene>> active_scenes_;
};

} // namespace ptgn