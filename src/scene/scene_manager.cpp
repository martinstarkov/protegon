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
		Get(scene_key)->Add(Scene::Action::Unload);
	}
}

void SceneManager::EnterImpl(
	const InternalKey& scene_key, const SceneTransition& scene_transition
) {
	PTGN_ASSERT(Has(scene_key), "Cannot set scene as active unless it has been loaded first");
	if (active_scene_ == nullptr && !game.IsRunning()) {
		active_scene_ = Get(scene_key);
		active_scene_->Init();
		// First active scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	} else {
		Get(scene_key)->Add(Scene::Action::Enter);
	}
	// TODO: Fix:
	// scene_transition.Start(false, from_scene_key, to_scene_key, Get(from_scene_key));
	// scene_transition.Start(true, to_scene_key, from_scene_key, Get(to_scene_key));
}

void SceneManager::UnloadAll() {
	auto& map{ GetMap() };
	for (auto& [key, scene] : map) {
		scene->Add(Scene::Action::Unload);
	}
}

Scene& SceneManager::GetActive() {
	PTGN_ASSERT(active_scene_ != nullptr, "There is no currently active scene");
	return *active_scene_;
}

const Scene& SceneManager::GetActive() const {
	PTGN_ASSERT(active_scene_ != nullptr, "There is no currently active scene");
	return *active_scene_;
}

void SceneManager::Reset() {
	UnloadAll();
	UpdateFlags();
	active_scene_ = nullptr;
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
			scene->target_.Draw(
				TextureInfo{ scene->tint_ }, LayerInfo{ 0, game.renderer.screen_target_ }
			);
		}
	}
	current_scene_ = nullptr;
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

} // namespace ptgn::impl
