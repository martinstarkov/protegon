#include "scene_manager.h"

#include "utility/debug.h"

namespace ptgn {

void SceneManager::Unload(std::size_t scene_key) {
	if (Has(scene_key)) {
		auto scene	   = Get(scene_key);
		scene->status_ = Scene::Status::Delete;
		flagged_++;
	}
}

void SceneManager::SetActive(std::size_t scene_key) {
	// ExitAllExcept(scene_key);
	PTGN_CHECK(
		Has(scene_key) || scene_key == impl::start_scene_key,
		"Cannot set active scene if it has not been loaded into the scene "
		"manager"
	);
	active_scenes_.clear();
	AddActive(scene_key);
}

void SceneManager::AddActive(std::size_t scene_key) {
	PTGN_ASSERT(
		Has(scene_key) || scene_key == impl::start_scene_key,
		"Cannot set scene to active unless it has been loaded first"
	);
	active_scenes_.emplace_back(scene_key);
	/*auto scene = Get(scene_key);
	scene->Enter();*/
}

void SceneManager::RemoveActive(std::size_t scene_key) {
	PTGN_CHECK(
		Has(scene_key), "Cannot remove active scene if it has not been loaded into "
				  "the scene manager"
	);
	for (auto it = active_scenes_.begin(); it != active_scenes_.end();) {
		if (*it == scene_key) {
			/*if (Has(scene_key)) {
				auto scene = Get(scene_key);
				scene->Exit();
			}*/
			it = active_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

std::vector<std::shared_ptr<Scene>> SceneManager::GetActive() {
	std::vector<std::shared_ptr<Scene>> active{};
	for (auto scene_key : active_scenes_) {
		if (Has(scene_key)) {
			auto scene = Get(scene_key);
			active.emplace_back(scene);
		}
	}
	return active;
}

void SceneManager::Update(float dt) {
	for (auto scene_key : active_scenes_) {
		if (Has(scene_key)) {
			auto scene = Get(scene_key);
			if (scene->status_ != Scene::Status::Delete) {
				scene->Update();
				scene->Update(dt);
			}
		}
	}
	UnloadFlagged();
}

void SceneManager::UnloadFlagged() {
	auto& map{ GetMap() };
	PTGN_ASSERT(flagged_ >= 0);
	while (flagged_ > 0) {
		for (auto it = map.begin(); it != map.end();) {
			if (it->second->status_ == Scene::Status::Delete) {
				it = map.erase(it);
				flagged_--;
			} else {
				++it;
			}
		}
	}
	PTGN_ASSERT(flagged_ == 0, "Could not delete a flagged scene");
}

bool SceneManager::ActiveScenesContain(std::size_t key) const {
	for (auto k : active_scenes_) {
		if (k == key) {
			return true;
		}
	}
	return false;
}

} // namespace ptgn