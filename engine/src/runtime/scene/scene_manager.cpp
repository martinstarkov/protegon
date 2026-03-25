#include "runtime/scene/scene_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/time/time.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_command.h"
#include "runtime/scene/scene_state.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

std::unordered_map<std::size_t, impl::SceneCommand> SceneManager::GetTopPriorityCommands() {
	std::unordered_map<std::size_t, std::vector<impl::SceneCommand>> grouped;

	for (auto& c : commands_) {
		grouped[c.to_scene_key].emplace_back(std::move(c));
	}
	commands_.clear();

	std::unordered_map<std::size_t, impl::SceneCommand> top_priority_commands;

	for (auto& [target_key, cmds] : grouped) {
		PTGN_ASSERT(!cmds.empty(), "Grouped scene commands cannot be empty");

		std::ranges::stable_sort(
			cmds,
			[](const impl::SceneCommand& a, const impl::SceneCommand& b) {
				auto type_rank = [](impl::SceneCommandType t) {
					switch (t) {
						using enum impl::SceneCommandType;
						case ReEnter: return 0;
						case Exit:	  return 1;
						case Enter:	  return 2;
						default:	  PTGN_ERROR("Unknown SceneCommandType: ", std::to_underlying(t));
					}
				};

				int ra = type_rank(a.type);
				int rb = type_rank(b.type);

				// First: group by type
				if (ra != rb) { // NOSONAR
					return ra < rb;
				}

				// Same type:
				if (a.type == impl::SceneCommandType::ReEnter) {
					return false; // preserve order (stable_sort keeps it)
				}

				// Exit & Enter: higher priority first
				return a.priority.value > b.priority.value;
			}
		);

		top_priority_commands[target_key] = std::move(cmds.front());
	}

	return top_priority_commands;
}

void SceneManager::ApplyCommands(
	std::unordered_map<std::size_t, impl::SceneCommand>& top_priority_commands
) {
	const auto enter = [this](auto target_key, auto& cmd) {
		auto new_scene = cmd.scene_factory();

		// TODO: Figure out delay system.
		// if (!cmd.use_delay) {
		//	s->scene_on_entered = true;
		//	newScene->InternalEnter();
		//}
		// newScene->use_delay = cmd.use_delay;

		new_scene->state_	   = impl::SceneState::TransitionIn;
		new_scene->transition_ = std::move(cmd.transition_in);
		if (new_scene->transition_) {
			new_scene->transition_->OnStart(*new_scene);
		}
		new_scene->key_ = target_key;
		new_scene->InternalEnter();

		scenes_.emplace_back(std::move(new_scene));
	};

	const auto exit = [this](auto target_key, auto& cmd) {
		auto& target_scene{ Get(target_key) };
		target_scene.state_		 = impl::SceneState::TransitionOut;
		target_scene.transition_ = std::move(cmd.transition_out);
		if (target_scene.transition_) {
			target_scene.transition_->OnStart(target_scene);
		}
	};

	for (auto& [target_key, cmd] : top_priority_commands) {
		switch (cmd.type) {
			case impl::SceneCommandType::Enter: {
				PTGN_ASSERT(
					!Has(target_key), "Cannot enter a scene which is already in the scene manager"
				);

				enter(target_key, cmd);
				break;
			}

			case impl::SceneCommandType::Exit: {
				PTGN_ASSERT(
					Has(target_key), "Cannot exit a scene which is not in the scene manager"
				);

				exit(target_key, cmd);
				break;
			}

			case impl::SceneCommandType::ReEnter: {
				PTGN_ASSERT(
					Has(target_key), "Cannot re-enter a scene which is not in the scene manager"
				);
				std::size_t temporary_key = target_key + 1;

				exit(target_key, cmd);
				enter(temporary_key, cmd);

				reentering_scenes_.emplace_back(temporary_key);
				break;
			}
		}
	}
}

void SceneManager::Update(secondsf dt) {
	for (const auto& scene : scenes_) {
		scene->InternalUpdate();
	}
	for (const auto& scene : scenes_) {
		scene->InternalDraw();
	}

	auto top_priority_commands = GetTopPriorityCommands();
	ApplyCommands(top_priority_commands);

	for (auto it = scenes_.begin(); it != scenes_.end();) {
		using enum impl::SceneState;

		const auto& scene = *it;
		if (scene->transition_) {
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
			// TODO: Add some sort of scene->OnTransitionFinished() callback so scenes can control
			// what appears in the scene when.
			// TODO: Add delay system.
			// if (!scene->scene_on_entered) {
			//	scene->InternalEnter();
			//	scene->scene_on_entered = true;
			//}
			++it;
		} else if (scene->state_ == TransitionOut) {
			scene->transition_.reset();
			scene->InternalExit();
			it = scenes_.erase(it);
		} else {
			++it;
		}
	}

	for (auto it = reentering_scenes_.begin(); it != reentering_scenes_.end();) {
		auto scene_key = *it;

		if (!Has(scene_key)) {
			++it;
			continue;
		}

		auto& scene{ Get(scene_key) };

		if (scene.state_ == impl::SceneState::Active) {
			scene.key_ = scene_key - 1;
			it		   = reentering_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

bool SceneManager::Has(std::size_t key) const {
	return std::ranges::any_of(scenes_, [key](const auto& scene) { return scene->key_ == key; });
}

const Scene& SceneManager::Get(std::size_t key) const {
	auto it =
		std::ranges::find_if(scenes_, [key](const auto& scene) { return scene->key_ == key; });
	PTGN_ASSERT(it != scenes_.end(), "Failed to retrieve scene with key: ", key);
	return *it->get();
}

Scene& SceneManager::Get(std::size_t key) {
	return const_cast<Scene&>(std::as_const(*this).Get(key)); // NOSONAR
}

} // namespace ptgn