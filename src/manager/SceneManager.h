#pragma once

#include <tuple> // std::pair
#include <vector> // std::vector
#include <cassert> // assert

#include "manager/ResourceManager.h"
#include "scene/Scene.h"

namespace ptgn {

class SceneManager : public manager::ResourceManager<Scene> {
public:
	void Unload(const std::size_t scene_key) {
		if (Has(scene_key)) {
			auto scene = Get(scene_key);
			scene->status_ = SceneStatus::DELETE;
			flagged_++;
		}
	}
	void SetActive(const std::size_t scene_key) {
		ExitAllExcept(scene_key);
		active_scenes_.clear();
		AddActive(scene_key);
	}
	void AddActive(const std::size_t scene_key) {
		active_scenes_.emplace_back(scene_key);
		assert(Has(scene_key) && "Cannot set scene to active unless it has been loaded first");
		auto scene = Get(scene_key);
		scene->Enter();
	}
	void RemoveActive(const std::size_t scene_key) {
		for (auto it = active_scenes_.begin(); it != active_scenes_.end();) {
			if (*it == scene_key) {
				if (Has(scene_key)) {
					auto scene = Get(scene_key);
					scene->Exit();
				}
				it = active_scenes_.erase(it);
			} else {
				++it;
			}
		}
	}
	std::vector<std::shared_ptr<Scene>> GetActive() {
		std::vector<std::shared_ptr<Scene>> active;
		for (auto scene_key : active_scenes_) {
			if (Has(scene_key)) {
				auto scene = Get(scene_key);
				active.emplace_back(scene);
			}
		}
		return active;
	}
	void Update(float dt) {
		for (auto scene_key : active_scenes_) {
			if (Has(scene_key)) {
				auto scene = Get(scene_key);
				if (scene->status_ != SceneStatus::DELETE)
					scene->Update(dt);
			}
		}
		UnloadFlagged();
	}
private:
	void UnloadFlagged() {
		if (flagged_ > 0) {
			auto& map{ GetMap() };
			for (auto it = map.begin(); it != map.end() && flagged_ > 0;) {
				if (it->second->status_ == SceneStatus::DELETE) {
					it = map.erase(it);
					flagged_--;
				} else {
					++it;
				}
			}
			assert(flagged_ == 0 && "Could not delete a flagged scene");
		}
	}
	void ExitAllExcept(const std::size_t scene_key) {
		for (auto other_key : active_scenes_) {
			if (other_key != scene_key && Has(other_key)) {
				auto scene = Get(other_key);
				scene->Exit();
			}
		}
	}
	bool ActiveScenesContain(const std::size_t key) const {
		for (auto k : active_scenes_)
			if (k == key) return true;
		return false;
	}
	std::size_t flagged_{ 0 };
	std::vector<std::size_t> active_scenes_;
};

} // namespace ptgn