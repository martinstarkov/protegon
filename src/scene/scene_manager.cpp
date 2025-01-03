#include "scene/scene_manager.h"

#include <algorithm>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/manager.h"
#include "event/input_handler.h"
#include "renderer/layer_info.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
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
	if (HasActiveSceneImpl(scene_key)) {
		return;
	}
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
	if (!HasActiveSceneImpl(scene_key)) {
		return;
	}
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

void SceneManager::TransitionActiveImpl(
	const InternalKey& from_scene_key, const InternalKey& to_scene_key,
	const SceneTransition& transition
) {
	if (transition == SceneTransition{}) {
		RemoveActiveImpl(from_scene_key);
		AddActiveImpl(to_scene_key);
		return;
	}

	if (HasActiveSceneImpl(to_scene_key)) {
		return;
	}

	transition.Start(false, from_scene_key, to_scene_key, Get(from_scene_key));
	transition.Start(true, to_scene_key, from_scene_key, Get(to_scene_key));
}

void SceneManager::SwitchActiveScenesImpl(const InternalKey& scene1, const InternalKey& scene2) {
	PTGN_ASSERT(
		HasActiveSceneImpl(scene1),
		"Cannot switch scene which does not exist in the active scene vector"
	);
	PTGN_ASSERT(
		HasActiveSceneImpl(scene2),
		"Cannot switch scene which does not exist in the active scene vector"
	);
	SwapVectorElements(active_scenes_, scene1, scene2);
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
	PTGN_ASSERT(!active_scenes_.empty(), "Cannot get top active scene, there are no active scenes");
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
		auto scene{ Get(scene_key) };
		if (scene->actions_.empty()) {
			currently_updating_ = scene;
			scene->Update();
			scene->target_.Flush();
		}
	}
	currently_updating_ = nullptr;
	for (auto scene_key : active_scenes_) {
		PTGN_ASSERT(Has(scene_key));
		auto scene{ Get(scene_key) };
		scene->target_.Draw({});
	}
	UpdateFlagged();
}

void SceneManager::SetSceneChanged(bool changed) {
	scene_changed_ = changed;
}

bool SceneManager::SceneChanged() const {
	return scene_changed_;
}

void SceneManager::UpdateFlagged() {
	SetSceneChanged(false);
	auto& map{ GetMap() };
	for (auto it{ map.begin() }; it != map.end();) {
		// Intentional reference counter increment to maintain scene during scene function calls.

		auto [key, scene] = *it;

		bool unload{ false };

		game.scene.currently_updating_ = scene;

		while (!scene->actions_.empty()) {
			auto action{ scene->actions_.begin() };
			switch (*action) {
				case Scene::Action::Preload:
					game.input.Reset();
					scene->Preload();
					break;
				case Scene::Action::Init:
					// Input is reset to ensure no previously pressed keys are considered held.
					game.input.Reset();
					// Each scene starts with a refreshed camera.
					scene->target_.GetCamera().ResetPrimary();
					scene->Init();
					SetSceneChanged(true);
					break;
				case Scene::Action::Shutdown:
					scene->Shutdown();
					SetSceneChanged(true);
					break;
				case Scene::Action::Unload:
					if (HasActiveSceneImpl(key)) {
						RemoveActiveImpl(key);
						PTGN_ASSERT(!HasActiveSceneImpl(key));
						continue;
					} else {
						scene->Unload();
						unload = true;
						SetSceneChanged(true);
					}
					break;
			}
			scene->actions_.erase(action);
		}

		game.scene.currently_updating_ = nullptr;

		if (unload) {
			it = map.erase(it);
		} else {
			++it;
		}
	}
}

bool SceneManager::HasActiveSceneImpl(const InternalKey& key) const {
	return VectorContains(active_scenes_, key);
}

} // namespace ptgn::impl
