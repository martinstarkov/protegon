#include "runtime/scene/scene_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/time.h"
#include "renderer/pipeline/draw_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

namespace impl {

std::unordered_map<std::size_t, SceneManager::Command> SceneManager::GetTopPriorityCommands() {
	std::unordered_map<std::size_t, std::vector<Command>> grouped;

	for (auto& c : commands_) {
		grouped[c.to_scene_tag_hash].emplace_back(std::move(c));
	}
	commands_.clear();

	// Key: target scene tag hash, Value: top priority command for the scene.
	std::unordered_map<std::size_t, Command> top_priority_commands;

	for (auto& [target_scene_tag_hash, cmds] : grouped) {
		PTGN_ASSERT(!cmds.empty(), "Grouped scene commands cannot be empty");

		std::ranges::stable_sort(cmds, [](const Command& a, const Command& b) {
			auto type_rank = [](CommandType t) {
				switch (t) {
					using enum CommandType;
					case ReEnter: return 0;
					case Exit:	  return 1;
					case Enter:	  return 2;
					default:	  PTGN_ERROR("Unknown CommandType: ", std::to_underlying(t));
				}
			};

			int ra = type_rank(a.type);
			int rb = type_rank(b.type);

			// First: group by type
			if (ra != rb) { // NOSONAR
				return ra < rb;
			}

			// Same type:
			if (a.type == CommandType::ReEnter) {
				return false; // preserve order (stable_sort keeps it)
			}

			// Exit & Enter: higher priority first
			return a.priority.value > b.priority.value;
		});

		top_priority_commands[target_scene_tag_hash] = std::move(cmds.front());
	}

	return top_priority_commands;
}

void SceneManager::ApplyCommands(
	Application& app, std::unordered_map<std::size_t, Command>& top_priority_commands
) {
	const auto enter = [this, &app](auto target_scene_tag_hash, auto& cmd) {
		auto new_scene{ std::invoke(
			cmd.scene_factory, app,
			SceneData{ .tag{ cmd.to_scene_tag },
					   .tag_hash{ target_scene_tag_hash },
					   .state{ SceneState::TransitionIn },
					   .transition{ std::move(cmd.transition_in) } }
		) };
		if (!new_scene->data_.transition) {
			new_scene->InternalEnter();
		} else {
			new_scene->data_.transition->OnDelayStart(*new_scene);
			if (!new_scene->data_.transition->IsInDelay()) {
				new_scene->data_.transition->started_ = true;
				new_scene->data_.transition->OnStart(*new_scene);
				new_scene->InternalEnter();
			}
		}

		scenes_.emplace_back(std::move(new_scene));
	};

	const auto exit = [this](auto target_scene_tag_hash, auto& cmd) {
		auto& target_scene{ GetScene(target_scene_tag_hash) };
		target_scene.data_.state	  = SceneState::TransitionOut;
		target_scene.data_.transition = std::move(cmd.transition_out);
		if (target_scene.data_.transition) {
			target_scene.data_.transition->OnDelayStart(target_scene);
			if (!target_scene.data_.transition->IsInDelay()) {
				target_scene.data_.transition->started_ = true;
				target_scene.data_.transition->OnStart(target_scene);
			}
		}
	};

	for (auto& [taget_scene_tag_hash, cmd] : top_priority_commands) {
		switch (cmd.type) {
			case CommandType::Enter: {
				PTGN_ASSERT(
					!HasScene(taget_scene_tag_hash),
					"Cannot enter a scene which is already in the scene manager"
				);

				enter(taget_scene_tag_hash, cmd);
				break;
			}

			case CommandType::Exit: {
				PTGN_ASSERT(
					HasScene(taget_scene_tag_hash),
					"Cannot exit a scene which is not in the scene manager"
				);

				exit(taget_scene_tag_hash, cmd);
				break;
			}

			case CommandType::ReEnter: {
				PTGN_ASSERT(
					HasScene(taget_scene_tag_hash),
					"Cannot re-enter a scene which is not in the scene manager"
				);

				std::size_t temporary_tag_hash{ GenerateTempTagHash() };

				exit(taget_scene_tag_hash, cmd);
				enter(temporary_tag_hash, cmd);

				reentering_scenes_.emplace_back(taget_scene_tag_hash, temporary_tag_hash);
				break;
			}
		}
	}
}

void SceneManager::PreUpdate() {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalPreUpdate();
	}
}

void SceneManager::OnEvent() {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalOnEvent();
	}
}

void SceneManager::Update(Application& app, secondsf dt) {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalUpdate();
	}
	auto top_priority_commands{ GetTopPriorityCommands() };
	ApplyCommands(app, top_priority_commands);
	UpdateTransitions(dt);
	UpdateReEnteredSceneTagHashes();
}

void SceneManager::UpdateTransitions(secondsf dt) {
	for (auto it = scenes_.begin(); it != scenes_.end();) {
		using enum SceneState;

		const auto& scene = *it;
		if (scene->data_.transition) {
			if (scene->data_.transition->IsInDelay()) {
				scene->data_.transition->UpdateDelayTime(dt);
				++it;
				continue;
			} else if (!scene->data_.transition->started_) {
				scene->data_.transition->started_ = true;
				scene->data_.transition->OnStart(*scene);
				if (scene->data_.state == TransitionIn) {
					scene->InternalEnter();
				}
			}

			scene->data_.transition->UpdateTime(dt);
			scene->data_.transition->OnUpdate(*scene);

			if (!scene->data_.transition->IsFinished()) {
				++it;
				continue;
			}
		}

		// Scene has no transition or transition is finished.

		if (scene->data_.transition) {
			scene->data_.transition->OnStop(*scene);
		}

		if (scene->data_.state == TransitionIn) {
			scene->data_.transition.reset();
			scene->data_.state = Active;
			++it;
		} else if (scene->data_.state == TransitionOut) {
			scene->data_.transition.reset();
			scene->InternalExit();
			it = scenes_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneManager::UpdateReEnteredSceneTagHashes() {
	for (auto it = reentering_scenes_.begin(); it != reentering_scenes_.end();) {
		if (!HasScene(it->temporary_scene_tag_hash)) {
			it = reentering_scenes_.erase(it);
			continue;
		}

		auto& scene{ GetScene(it->temporary_scene_tag_hash) };

		if (scene.data_.state == SceneState::Active) {
			scene.data_.tag_hash = it->scene_tag_hash;
			it					 = reentering_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneManager::Draw(DrawContext& draw_context) const {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalDraw(draw_context);
	}
}

std::size_t SceneManager::GenerateTempTagHash() const {
	static std::size_t next_temp_tag_hash{ 1 };
	while (HasScene(next_temp_tag_hash)) {
		++next_temp_tag_hash;
	}
	return next_temp_tag_hash;
}

bool SceneManager::HasScene(std::size_t scene_tag_hash) const {
	return std::ranges::any_of(scenes_, [scene_tag_hash](const auto& scene) {
		return scene->data_.tag_hash == scene_tag_hash;
	});
}

const Scene& SceneManager::GetScene(std::size_t scene_tag_hash) const {
	auto it = std::ranges::find_if(scenes_, [scene_tag_hash](const auto& scene) {
		return scene->data_.tag_hash == scene_tag_hash;
	});
	PTGN_ASSERT(it != scenes_.end(), "Failed to retrieve scene with tag hash: ", scene_tag_hash);
	return *it->get();
}

Scene& SceneManager::GetScene(std::size_t scene_tag_hash) {
	return const_cast<Scene&>(std::as_const(*this).GetScene(scene_tag_hash)); // NOSONAR
}

const std::vector<std::unique_ptr<Scene>>& SceneManager::GetScenes() const {
	return scenes_;
}

std::vector<std::unique_ptr<Scene>>& SceneManager::GetScenes() {
	return scenes_;
}

bool SceneManager::CanIssueCommands(std::size_t target_scene_tag_hash) const {
	if (!HasScene(target_scene_tag_hash)) {
		return true;
	}

	if (const auto& target_scene{ GetScene(target_scene_tag_hash) };
		target_scene.IsTransitioning()) {
		return false;
	}

	return true;
}

} // namespace impl

LocalSceneManager::LocalSceneManager(impl::SceneManager& scene_manager, Scene& scene) :
	scene_manager_{ scene_manager }, scene_{ scene } {}

bool LocalSceneManager::CanIssueCommands() const {
	return !scene_.IsTransitioning();
}

} // namespace ptgn