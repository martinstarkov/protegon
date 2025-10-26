#include "world/scene/scene_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/app/game.h"
#include "core/app/manager.h"
#include "core/input/input_handler.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/file.h"
#include "core/util/span.h"
#include "debug/runtime/assert.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "tweens/tween.h"
#include "ui/menu_template.h"
#include "world/scene/scene.h"
#include "world/scene/scene_key.h"
#include "world/scene/scene_transition.h"

namespace ptgn::impl {

template <typename Container>
static auto FindSceneIt(Container& container, const SceneKey& key) {
	auto it{ std::ranges::find_if(container, [&key](const auto& scene) {
		return scene->GetKey() == key;
	}) };
	PTGN_ASSERT(it != container.end(), "Scene ", key.GetKey(), " not found in scene manager");
	return it;
}

const Scene& SceneManager::GetCurrent() const {
	PTGN_ASSERT(current_, "Cannot get current scene when one has not been set");
	return *current_;
}

Scene& SceneManager::GetCurrent() {
	return const_cast<Scene&>(std::as_const(*this).GetCurrent());
}

bool SceneManager::Has(const SceneKey& scene_key) const {
	return GetImpl(scene_key) != nullptr;
}

bool SceneManager::IsActive(const SceneKey& scene_key) const {
	return VectorFindIf(active_scenes_, [scene_key](const auto& scene) {
		return scene->GetKey() == scene_key;
	});
}

void SceneManager::ExitAll() {
	for (const auto& scene : active_scenes_) {
		Exit(scene->GetKey());
	}
}

std::shared_ptr<Scene> SceneManager::GetImpl(const SceneKey& scene_key) const {
	auto it{ std::ranges::find_if(scenes_, [&scene_key](const auto& scene) {
		return scene->GetKey() == scene_key;
	}) };
	if (it == scenes_.end()) {
		return nullptr;
	}
	return *it;
}

void SceneManager::Unload(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };
	if (scene) {
		scene->state_ = Scene::State::Unloading;
	}
}

void SceneManager::Enter(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };
	PTGN_ASSERT(scene, "Cannot enter a scene unless it has been loaded first");

	scene->state_ = Scene::State::Entering;

	if (scene->first_scene_ && active_scenes_.empty()) {
		// First active scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	}
}

void SceneManager::Exit(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };

	if (!scene) {
		return;
	}

	if (!IsActive(scene_key)) {
		return;
	}

	scene->state_ = Scene::State::Exiting;
}

void SceneManager::Reset() {
	for (auto& scene : scenes_) {
		scene->state_ = Scene::State::Unloading;
	}
	HandleSceneEvents();
	scenes_		   = {};
	active_scenes_ = {};
	current_	   = nullptr;
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::Update(Game& g) {
	HandleSceneEvents();

	if (active_scenes_.empty()) {
		return;
	}

	g.renderer.ClearScreen();

	auto& render_data{ g.renderer.render_data_ };

	g.input.Update();

	// TODO: Figure out a better way to do non-scene events / scripts.

	bool invoke_actions{ render_data.game_size_changed_ || render_data.display_size_changed_ };

	const auto invoke_resolution_events = [&](Manager& manager) {
		manager.Refresh();

		if (render_data.game_size_changed_) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.AddAction(&GameSizeScript::OnGameSizeChanged);
			}
		}
		if (render_data.display_size_changed_) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.AddAction(&DisplaySizeScript::OnDisplaySizeChanged);
			}
		}
		if (invoke_actions) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.InvokeActions();
			}
		}

		manager.Refresh();
	};

	g.input.InvokeInputEvents(render_data.render_manager);
	invoke_resolution_events(render_data.render_manager);

	Tween::Update(render_data.render_manager, g.dt());

	auto scenes{ scenes_ };
	for (auto scene : scenes) {
		invoke_resolution_events(*scene);
	}

	render_data.game_size_changed_	  = false;
	render_data.display_size_changed_ = false;

	auto active_scenes{ active_scenes_ };
	for (auto active_scene : active_scenes) {
		current_ = active_scene;
		active_scene->InternalUpdate();
		current_ = nullptr;
	}

	render_data.DrawScreenTarget();

	g.renderer.PresentScreen();
}

void SceneManager::HandleSceneEvents() {
	// If bool is true, erase scene from active vector.
	std::vector<std::pair<bool, std::shared_ptr<Scene>>> exit;
	// If bool is true, add scene to active vector.
	std::vector<std::pair<bool, std::shared_ptr<Scene>>> enter;
	std::vector<std::shared_ptr<Scene>> unload;

	for (auto scene : scenes_) {
		switch (scene->state_) {
			case Scene::State::Running:		break;
			case Scene::State::Constructed: break;
			case Scene::State::Paused:		break;
			case Scene::State::Sleeping:	break;
			case Scene::State::Entering:	{
				if (IsActive(scene->GetKey())) {
					exit.emplace_back(false, scene);
					enter.emplace_back(false, scene);
				} else {
					enter.emplace_back(true, scene);
				}
				scene->state_ = Scene::State::Running;
				break;
			}
			case Scene::State::Exiting:
				if (scene->transition_) {
					if (!scene->transition_->HasStarted()) {
						scene->transition_->Start();
					}
					// Wait for transition to exit.
					break;
				}
				exit.emplace_back(true, scene);
				scene->state_ = Scene::State::Constructed;
				break;
			case Scene::State::Unloading: {
				if (IsActive(scene->GetKey())) {
					exit.emplace_back(true, scene);
				}
				unload.emplace_back(scene);
				break;
			}
		}
	}
	for (auto [erase_active, scene] : exit) {
		current_ = scene;
		scene->InternalExit();
		current_ = nullptr;
		if (erase_active) {
			VectorErase(active_scenes_, scene);
		}
	}
	for (auto [add_active, scene] : enter) {
		if (add_active) {
			active_scenes_.emplace_back(scene);
		}
		current_ = scene;
		scene->InternalEnter();
		if (scene->transition_) {
			scene->transition_->Start();
		}
		current_ = nullptr;
	}
	for (auto scene : unload) {
		VectorErase(scenes_, scene);
	}
}

void SceneManager::MoveUp(const SceneKey& scene_key) {
	auto it{ FindSceneIt(scenes_, scene_key) };
	if (it != scenes_.begin()) {
		std::iter_swap(it, std::prev(it));
	}
}

void SceneManager::MoveDown(const SceneKey& scene_key) {
	auto it{ FindSceneIt(scenes_, scene_key) };
	if (std::next(it) != scenes_.end()) {
		std::iter_swap(it, std::next(it));
	}
}

void SceneManager::BringToTop(const SceneKey& scene_key) {
	auto it{ FindSceneIt(scenes_, scene_key) };
	if (it != std::prev(scenes_.end())) {
		auto scene{ *it };
		scenes_.erase(it);
		scenes_.push_back(scene);
	}
}

void SceneManager::MoveToBottom(const SceneKey& scene_key) {
	auto it{ FindSceneIt(scenes_, scene_key) };
	if (it != scenes_.begin()) {
		auto scene{ *it };
		scenes_.erase(it);
		scenes_.insert(scenes_.begin(), scene);
	}
}

void SceneManager::MoveAbove(const SceneKey& source_key, const SceneKey& target_key) {
	if (source_key == target_key) {
		return;
	}

	auto source_it{ FindSceneIt(scenes_, source_key) };
	auto target_it{ FindSceneIt(scenes_, target_key) };

	auto scene{ *source_it };
	scenes_.erase(source_it);

	// Recalculate target_it in case source was before target and got erased.
	target_it = FindSceneIt(scenes_, target_key);

	// Insert before target.
	scenes_.insert(target_it, scene);
}

void SceneManager::MoveBelow(const SceneKey& source_key, const SceneKey& target_key) {
	if (source_key == target_key) {
		return;
	}

	auto source_it{ FindSceneIt(scenes_, source_key) };
	auto target_it{ FindSceneIt(scenes_, target_key) };

	auto scene{ *source_it };
	scenes_.erase(source_it);

	// Recalculate target_it in case source was before target.
	target_it = FindSceneIt(scenes_, target_key);

	// Insert after target.
	scenes_.insert(std::next(target_it), scene);
}

void SceneManager::EnterConfig(const path& scene_json_file) {
	json j = LoadJson(scene_json_file);

	PTGN_ASSERT(j.contains("scenes"), "Scene config must contain a scenes dictionary");
	PTGN_ASSERT(j.contains("start_scene"), "Scene config must specify a start scene");

	const json& scene_json = j.at("scenes");

	auto start_scene{ j.at("start_scene").get<std::string>() };

	PTGN_ASSERT(scene_json.contains(start_scene), "Start scene must be in the scenes dictionary");

	game.scene.Enter<TemplateMenuScene>(start_scene, start_scene, scene_json);
}

} // namespace ptgn::impl