#include "scene/scene_manager.h"

#include <algorithm>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "core/manager.h"
#include "event/input_handler.h"
#include "protegon/game.h"
#include "protegon/scene.h"
#include "scene/camera.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn::impl {

void SceneManager::UnloadImpl(const InternalKey& scene_key) {
	if (Has(scene_key)) {
		auto scene = Get(scene_key);
		scene->Add(Scene::Action::Unload);
	}
}

void SceneManager::InitScene(const InternalKey& scene_key) {
	PTGN_ASSERT(
		Has(scene_key) || scene_key == impl::start_scene_key,
		"Cannot init scene unless it has been loaded first"
	);
	auto scene = Get(scene_key);
	scene->Add(Scene::Action::Init);
}

void SceneManager::AddActiveImpl(const InternalKey& scene_key) {
	PTGN_ASSERT(
		Has(scene_key) || scene_key == impl::start_scene_key,
		"Cannot add scene to active unless it has been loaded first"
	);
	active_scenes_.emplace_back(scene_key);
	// Start scene is initialized manually in the game.
	if (scene_key != impl::start_scene_key) {
		InitScene(scene_key);
	}
}

void SceneManager::ClearActive() {
	for (auto it = active_scenes_.begin(); it != active_scenes_.end();) {
		Get(*it)->Add(Scene::Action::Shutdown);
		it = active_scenes_.erase(it);
	}
	PTGN_ASSERT(active_scenes_.empty());
}

void SceneManager::UnloadAll() {
	auto& map{ GetMap() };
	for (auto it = map.begin(); it != map.end(); ++it) {
		it->second->Add(Scene::Action::Unload);
	}
}

void SceneManager::RemoveActiveImpl(const InternalKey& scene_key) {
	PTGN_ASSERT(
		Has(scene_key), "Cannot remove active scene if it has not been loaded into "
						"the scene manager"
	);
	for (auto it = active_scenes_.begin(); it != active_scenes_.end();) {
		if (*it == scene_key) {
			Get(scene_key)->Add(Scene::Action::Shutdown);
			it = active_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

std::vector<std::shared_ptr<Scene>> SceneManager::GetActive() {
	std::vector<std::shared_ptr<Scene>> active{};
	for (auto scene_key : active_scenes_) {
		PTGN_ASSERT(Has(scene_key));
		active.emplace_back(Get(scene_key));
	}

	return active;
}

Scene& SceneManager::GetTopActive() {
	auto scene_key = active_scenes_.back();
	PTGN_ASSERT(Has(scene_key));
	auto scene = Get(scene_key);
	return *scene;
}

void SceneManager::Reset() {
	ClearActive();
	UnloadAll();
	UpdateFlagged();
	active_scenes_ = {};
	MapManager::Reset();
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::Update() {
	for (auto scene_key : active_scenes_) {
		PTGN_ASSERT(Has(scene_key));
		auto scene = Get(scene_key);
		scene->dt  = game.dt();
		if (scene->actions_.empty()) {
			scene->Update();
		}
	}
}

bool SceneManager::UpdateFlagged() {
	bool scene_change{ false };
	auto& map{ GetMap() };
	for (auto it = map.begin(); it != map.end();) {
		// Intentional reference counter increment to maintain scene during scene function calls.
		auto [key, scene] = *it;

		bool unload{ false };

		while (!scene->actions_.empty()) {
			auto action = scene->actions_.begin();
			switch (*action) {
				case Scene::Action::Preload:
					game.input.Reset();
					scene->Preload();
					break;
				case Scene::Action::Init:
					// Input is reset to ensure no previously pressed keys are considered held.
					game.input.Reset();
					// Each scene starts with a refreshed camera.
					scene->camera.ResetPrimary();
					scene->Init();
					scene_change = true;
					break;
				case Scene::Action::Shutdown:
					scene->Shutdown();
					scene_change = true;
					break;
				case Scene::Action::Unload:
					if (ActiveScenesContain(key)) {
						RemoveActiveImpl(key);
						PTGN_ASSERT(!ActiveScenesContain(key));
						continue;
					} else {
						scene->Unload();
						unload		 = true;
						scene_change = true;
					}
					break;
			}
			scene->actions_.erase(action);
		}

		if (unload) {
			it = map.erase(it);
		} else {
			++it;
		}
	}
	return scene_change;
}

bool SceneManager::ActiveScenesContain(const InternalKey& key) const {
	return VectorContains(active_scenes_, key);
}

} // namespace ptgn::impl
