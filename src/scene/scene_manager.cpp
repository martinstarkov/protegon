#include "scene/scene_manager.h"

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/manager.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "renderer/layer_info.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
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
	PTGN_ASSERT(Has(scene_key), "Cannot initialize a scene unless it has been loaded first");
	auto scene = Get(scene_key);
	scene->Add(Scene::Action::Init);
}

void SceneManager::AddActiveImpl(const InternalKey& scene_key, bool first_scene) {
	if (HasActiveSceneImpl(scene_key)) {
		return;
	}
	PTGN_ASSERT(Has(scene_key), "Cannot add scene to active unless it has been loaded first");

	active_scenes_.emplace_back(scene_key);
	InitScene(scene_key);

	if (first_scene) {
		// First active scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	}
}

void SceneManager::ClearActive() {
	for (auto it = active_scenes_.begin(); it != active_scenes_.end();) {
		Get(*it)->Add(Scene::Action::Shutdown);
		it = active_scenes_.erase(it);
	}
	PTGN_ASSERT(active_scenes_.empty(), "Failed to clear active scenes");
}

void SceneManager::UnloadAllScenes() {
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
	PTGN_ASSERT(
		HasActiveSceneImpl(from_scene_key),
		"Cannot transition from a scene which is not currently active"
	);
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

std::vector<std::shared_ptr<Scene>> SceneManager::GetActiveScenes() {
	std::vector<std::shared_ptr<Scene>> active{};
	for (auto scene_key : active_scenes_) {
		PTGN_ASSERT(
			Has(scene_key),
			"Attempting to retrieve active scene which is not loaded in the scene manager"
		);
		active.emplace_back(Get(scene_key));
	}

	return active;
}

Scene& SceneManager::GetTopActive() {
	PTGN_ASSERT(!active_scenes_.empty(), "Cannot get top active scene, there are no active scenes");
	auto scene_key{ active_scenes_.back() };
	PTGN_ASSERT(
		Has(scene_key),
		"Cannot retrieve top active scene which has not been loaded into the scene manager"
	);
	auto scene{ Get(scene_key) };
	return *scene;
}

void SceneManager::Reset() {
	UnloadAllScenes();
	UpdateFlagged();
	active_scenes_ = {};
	MapManager::Reset();
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::Update() {
	for (auto scene_key : active_scenes_) {
		if (!Has(scene_key)) {
			continue;
		}
		auto scene{ Get(scene_key) };
		if (scene->actions_.empty()) {
			current_scene_ = scene;
			scene->Update();
			scene->target_.Flush();
		}
	}
	current_scene_ = nullptr;
	std::int32_t layer{ 0 };
	for (auto scene_key : active_scenes_) {
		PTGN_ASSERT(
			Has(scene_key),
			"Attempting to draw an active scene which has not been loaded into the scene manager"
		);
		auto scene{ Get(scene_key) };
		scene->target_.Draw(
			TextureInfo{ scene->tint_ }, LayerInfo{ layer, game.renderer.screen_target_ }
		);
		layer++;
	}
}

Scene& SceneManager::GetCurrent() {
	PTGN_ASSERT(HasCurrent(), "No currently active scene has been set");
	return *current_scene_;
}

const Scene& SceneManager::GetCurrent() const {
	PTGN_ASSERT(HasCurrent(), "No currently active scene has been set");
	return *current_scene_;
}

CameraManager& SceneManager::GetCurrentCamera() {
	return game.renderer.GetCurrentRenderTarget().GetCamera();
}

const CameraManager& SceneManager::GetCurrentCamera() const {
	return game.renderer.GetCurrentRenderTarget().GetCamera();
}

bool SceneManager::HasCurrent() const {
	return current_scene_ != nullptr;
}

void SceneManager::UpdateFlagged() {
	auto& map{ GetMap() };
	for (auto it{ map.begin() }; it != map.end();) {
		// Intentional reference counter increment to maintain scene during scene function calls.
		auto [key, scene] = *it;

		bool unload{ false };

		while (!scene->actions_.empty()) {
			current_scene_ = scene;
			auto action{ scene->actions_.begin() };
			switch (*action) {
				case Scene::Action::Init:
					// Input is reset to ensure no previously pressed keys are considered held.
					game.input.ResetKeyStates();
					game.input.ResetMouseStates();
					scene->Init();
					break;
				case Scene::Action::Shutdown: scene->Shutdown(); break;
				case Scene::Action::Unload:
					if (HasActiveSceneImpl(key)) {
						RemoveActiveImpl(key);
						PTGN_ASSERT(!HasActiveSceneImpl(key), "Failed to remove active scene");
						continue;
					} else {
						scene->Unload();
						unload = true;
					}
					break;
			}
			scene->actions_.erase(action);
		}

		current_scene_ = nullptr;

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
