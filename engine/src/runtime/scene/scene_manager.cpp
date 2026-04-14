#include "runtime/scene/scene_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/log.h"
#include "core/time/time.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_state.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

namespace impl {

std::unordered_map<std::size_t, SceneManager::Command> SceneManager::GetTopPriorityCommands() {
	std::unordered_map<std::size_t, std::vector<Command>> grouped;

	for (auto& c : commands_) {
		grouped[c.to_scene_key].emplace_back(std::move(c));
	}
	commands_.clear();

	std::unordered_map<std::size_t, Command> top_priority_commands;

	for (auto& [target_key, cmds] : grouped) {
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

		top_priority_commands[target_key] = std::move(cmds.front());
	}

	return top_priority_commands;
}

void SceneManager::ApplyCommands(std::unordered_map<std::size_t, Command>& top_priority_commands) {
	const auto enter = [this](auto target_key, auto& cmd) {
		auto new_scene = cmd.scene_factory();

		new_scene->state_	   = SceneState::TransitionIn;
		new_scene->key_		   = target_key;
		new_scene->transition_ = std::move(cmd.transition_in);
		if (!new_scene->transition_) {
			new_scene->InternalEnter();
		} else {
			new_scene->transition_->OnDelayStart(*new_scene);
			if (!new_scene->transition_->IsInDelay()) {
				new_scene->transition_->started_ = true;
				new_scene->transition_->OnStart(*new_scene);
				new_scene->InternalEnter();
			}
		}

		scenes_.emplace_back(std::move(new_scene));
	};

	const auto exit = [this](auto target_key, auto& cmd) {
		auto& target_scene{ GetScene(target_key) };
		target_scene.state_		 = SceneState::TransitionOut;
		target_scene.transition_ = std::move(cmd.transition_out);
		if (target_scene.transition_) {
			target_scene.transition_->OnDelayStart(target_scene);
			if (!target_scene.transition_->IsInDelay()) {
				target_scene.transition_->started_ = true;
				target_scene.transition_->OnStart(target_scene);
			}
		}
	};

	for (auto& [target_key, cmd] : top_priority_commands) {
		switch (cmd.type) {
			case CommandType::Enter: {
				PTGN_ASSERT(
					!HasScene(target_key),
					"Cannot enter a scene which is already in the scene manager"
				);

				enter(target_key, cmd);
				break;
			}

			case CommandType::Exit: {
				PTGN_ASSERT(
					HasScene(target_key), "Cannot exit a scene which is not in the scene manager"
				);

				exit(target_key, cmd);
				break;
			}

			case CommandType::ReEnter: {
				PTGN_ASSERT(
					HasScene(target_key),
					"Cannot re-enter a scene which is not in the scene manager"
				);

				std::size_t temporary_key{ GenerateTempKey() };

				exit(target_key, cmd);
				enter(temporary_key, cmd);

				reentering_scenes_.emplace_back(target_key, temporary_key);
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

void SceneManager::OnEvent(std::vector<impl::EventData> events) {
	for (const auto& global_event : events) {
		for (const auto& scene : scenes_) {
			if (scene->IsAwaitingTransitionDelay()) {
				continue;
			}
			Event event{ global_event };
			scene->InternalOnEvent(event);
		}
	}
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalOnEvent();
	}
}

void SceneManager::Update(secondsf dt) {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalUpdate();
	}
	auto top_priority_commands{ GetTopPriorityCommands() };
	ApplyCommands(top_priority_commands);
	UpdateTransitions(dt);
	UpdateReEnteredSceneKeys();
}

void SceneManager::UpdateTransitions(secondsf dt) {
	for (auto it = scenes_.begin(); it != scenes_.end();) {
		using enum SceneState;

		const auto& scene = *it;
		if (scene->transition_) {
			if (scene->transition_->IsInDelay()) {
				scene->transition_->UpdateDelayTime(dt);
				++it;
				continue;
			} else if (!scene->transition_->started_) {
				scene->transition_->started_ = true;
				scene->transition_->OnStart(*scene);
				if (scene->state_ == TransitionIn) {
					scene->InternalEnter();
				}
			}

			scene->transition_->UpdateTime(dt);
			scene->transition_->OnUpdate(*scene);

			if (!scene->transition_->IsFinished()) {
				++it;
				continue;
			}
		}

		// Scene has no transition or transition is finished.

		if (scene->transition_) {
			scene->transition_->OnStop(*scene);
		}

		if (scene->state_ == TransitionIn) {
			scene->transition_.reset();
			scene->state_ = Active;
			++it;
		} else if (scene->state_ == TransitionOut) {
			scene->transition_.reset();
			scene->InternalExit();
			it = scenes_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneManager::UpdateReEnteredSceneKeys() {
	for (auto it = reentering_scenes_.begin(); it != reentering_scenes_.end();) {
		if (!HasScene(it->temporary_scene_key)) {
			it = reentering_scenes_.erase(it);
			continue;
		}

		auto& scene{ GetScene(it->temporary_scene_key) };

		if (scene.state_ == SceneState::Active) {
			scene.key_ = it->scene_key;
			it		   = reentering_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneManager::Draw() const {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalDraw();
	}
}

std::size_t SceneManager::GenerateTempKey() const {
	static std::size_t next_temp_key{ 1 };
	while (HasScene(next_temp_key)) {
		++next_temp_key;
	}
	return next_temp_key;
}

bool SceneManager::HasScene(std::size_t scene_key) const {
	return std::ranges::any_of(scenes_, [scene_key](const auto& scene) {
		return scene->key_ == scene_key;
	});
}

const Scene& SceneManager::GetScene(std::size_t scene_key) const {
	auto it = std::ranges::find_if(scenes_, [scene_key](const auto& scene) {
		return scene->key_ == scene_key;
	});
	PTGN_ASSERT(it != scenes_.end(), "Failed to retrieve scene with key: ", scene_key);
	return *it->get();
}

Scene& SceneManager::GetScene(std::size_t scene_key) {
	return const_cast<Scene&>(std::as_const(*this).GetScene(scene_key)); // NOSONAR
}

const std::vector<std::unique_ptr<Scene>>& SceneManager::GetScenes() const {
	return scenes_;
}

} // namespace impl

} // namespace ptgn