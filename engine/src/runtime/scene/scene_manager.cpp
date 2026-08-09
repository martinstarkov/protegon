#include "runtime/scene/scene_manager.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/application_context.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "renderer/draw_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

namespace impl {

bool SceneManager::EnterFactory(
	std::string_view scene_tag, SceneFactory scene_factory, SceneTransitionPriority priority
) {
	auto scene_tag_hash{ Hash(scene_tag) };
	if (!CanIssueCommands(scene_tag_hash)) {
		return false;
	}

	if (HasScene(scene_tag_hash)) {
		return ReEnterFactory(scene_tag, std::move(scene_factory));
	}

	PushCommand(
		CommandType::Enter, std::string{ scene_tag }, scene_tag_hash, priority,
		std::move(scene_factory), nullptr, nullptr
	);
	return true;
}

bool SceneManager::ReEnterFactory(std::string_view scene_tag, SceneFactory scene_factory) {
	auto scene_tag_hash{ Hash(scene_tag) };
	if (!CanIssueCommands(scene_tag_hash)) {
		return false;
	}

	PTGN_ASSERT(
		HasScene(scene_tag_hash), "Cannot re-enter a scene tag hash which has not been entered"
	);

	PushCommand(
		CommandType::ReEnter, std::string{ scene_tag }, scene_tag_hash,
		SceneTransitionPriority{ std::numeric_limits<std::size_t>::max() },
		std::move(scene_factory), nullptr, nullptr
	);
	return true;
}

bool SceneManager::RenameScene(
	std::string_view current_key,
	std::string_view new_key
) {
	if (current_key.empty() || new_key.empty()) {
		return false;
	}

	const auto current_hash{ Hash(current_key) };
	const auto new_hash{ Hash(new_key) };

	if (!HasScene(current_hash)) {
		return false;
	}

	if (current_hash == new_hash) {
		auto& scene{ GetScene(current_hash) };
		scene.data_.tag = std::string{ new_key };
		return true;
	}

	if (HasScene(new_hash) ||
		!CanIssueCommands(current_hash)) {
		return false;
	}

	for (const auto& command : commands_) {
		if (command.to_scene_tag_hash == current_hash ||
			command.to_scene_tag_hash == new_hash) {
			return false;
		}
	}

	auto& scene{ GetScene(current_hash) };
	scene.data_.tag = std::string{ new_key };
	scene.data_.tag_hash = new_hash;

	for (auto& reentering : reentering_scenes_) {
		if (reentering.scene_tag_hash == current_hash) {
			reentering.scene_tag_hash = new_hash;
		}
	}

	return true;
}

bool SceneManager::MoveScene(
	std::size_t from_index,
	std::size_t to_index
) {
	if (from_index >= scenes_.size() ||
		to_index >= scenes_.size()) {
		return false;
	}

	if (from_index == to_index) {
		return true;
	}

	if (from_index < to_index) {
		std::rotate(
			scenes_.begin() + static_cast<std::ptrdiff_t>(from_index),
			scenes_.begin() + static_cast<std::ptrdiff_t>(from_index + 1),
			scenes_.begin() + static_cast<std::ptrdiff_t>(to_index + 1)
		);
	} else {
		std::rotate(
			scenes_.begin() + static_cast<std::ptrdiff_t>(to_index),
			scenes_.begin() + static_cast<std::ptrdiff_t>(from_index),
			scenes_.begin() + static_cast<std::ptrdiff_t>(from_index + 1)
		);
	}

	return true;
}

void SceneManager::ReorderScenes(
	std::span<const std::string> ordered_keys,
	bool runtime
) {
	std::unordered_map<std::string_view, std::size_t> order;
	order.reserve(ordered_keys.size());

	for (std::size_t index{ 0 }; index < ordered_keys.size(); ++index) {
		order.emplace(ordered_keys[index], index);
	}

	std::vector<std::size_t> matching_slots;
	std::vector<std::unique_ptr<Scene>> matching_scenes;

	for (std::size_t index{ 0 }; index < scenes_.size(); ++index) {
		if (scenes_[index] &&
			scenes_[index]->IsRuntime() == runtime) {
			matching_slots.emplace_back(index);
			matching_scenes.emplace_back(std::move(scenes_[index]));
		}
	}

	std::ranges::stable_sort(
		matching_scenes,
		[&order](const auto& a, const auto& b) {
			const auto a_it{ order.find(a->GetTag()) };
			const auto b_it{ order.find(b->GetTag()) };

			const auto a_order{
				a_it == order.end()
					? std::numeric_limits<std::size_t>::max()
					: a_it->second
			};
			const auto b_order{
				b_it == order.end()
					? std::numeric_limits<std::size_t>::max()
					: b_it->second
			};

			return a_order < b_order;
		}
	);

	for (std::size_t index{ 0 }; index < matching_slots.size(); ++index) {
		scenes_[matching_slots[index]] =
			std::move(matching_scenes[index]);
	}
}

std::unordered_map<std::size_t, SceneManager::Command> SceneManager::GetTopPriorityCommands() {
	std::unordered_map<std::size_t, std::vector<Command>> grouped;

	for (auto& command : commands_) {
		grouped[command.to_scene_tag_hash].emplace_back(std::move(command));
	}
	commands_.clear();

	std::unordered_map<std::size_t, Command> top_priority_commands;

	for (auto& [target_scene_tag_hash, commands] : grouped) {
		PTGN_ASSERT(!commands.empty(), "Grouped scene commands cannot be empty");

		std::ranges::stable_sort(commands, [](const Command& a, const Command& b) {
			auto type_rank = [](CommandType type) {
				switch (type) {
					using enum CommandType;
					case ReEnter: return 0;
					case Exit:    return 1;
					case Enter:   return 2;
					default:      PTGN_ERROR("Unknown CommandType: ", std::to_underlying(type));
				}
			};

			int a_rank{ type_rank(a.type) };
			int b_rank{ type_rank(b.type) };

			if (a_rank != b_rank) {
				return a_rank < b_rank;
			}

			if (a.type == CommandType::ReEnter) {
				return false;
			}

			return a.priority.value > b.priority.value;
		});

		top_priority_commands[target_scene_tag_hash] = std::move(commands.front());
	}

	return top_priority_commands;
}

void SceneManager::ApplyLoadedCommand(
	Application& app,
	Command command,
	impl::AssetLoadTicket ticket
) {
	const auto enter = [this, &app](
		std::size_t target_scene_tag_hash,
		Command& enter_command,
		impl::AssetLoadTicket& asset_ticket
	) {
		auto new_scene{ enter_command.scene_factory(
			app,
			SceneData{
				.tag{ enter_command.to_scene_tag },
				.tag_hash{ target_scene_tag_hash },
				.state{ SceneState::TransitionIn },
				.transition{ std::move(enter_command.transition_in) },
			}
		) };

		PTGN_ASSERT(new_scene, "Scene factory returned null: ", enter_command.to_scene_tag);
		new_scene->AdoptLoadedAssetDependencies(asset_ticket.ReleaseOwnership());

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

	const auto exit = [this](std::size_t target_scene_tag_hash, Command& exit_command) {
		auto& target_scene{ GetScene(target_scene_tag_hash) };
		target_scene.data_.state = SceneState::TransitionOut;
		target_scene.data_.transition = std::move(exit_command.transition_out);
		if (target_scene.data_.transition) {
			target_scene.data_.transition->OnDelayStart(target_scene);
			if (!target_scene.data_.transition->IsInDelay()) {
				target_scene.data_.transition->started_ = true;
				target_scene.data_.transition->OnStart(target_scene);
			}
		}
	};

	const auto target_scene_tag_hash{ command.to_scene_tag_hash };

	switch (command.type) {
		case CommandType::Enter:
			PTGN_ASSERT(
				!HasScene(target_scene_tag_hash),
				"Cannot enter a scene which is already in the scene manager"
			);
			enter(target_scene_tag_hash, command, ticket);
			break;

		case CommandType::ReEnter: {
			PTGN_ASSERT(
				HasScene(target_scene_tag_hash),
				"Cannot re-enter a scene which is not in the scene manager"
			);

			const std::size_t temporary_tag_hash{ GenerateTempTagHash() };
			exit(target_scene_tag_hash, command);
			enter(temporary_tag_hash, command, ticket);
			reentering_scenes_.emplace_back(target_scene_tag_hash, temporary_tag_hash);
			break;
		}

		case CommandType::Exit:
			PTGN_ERROR("Loaded scene command cannot be an exit command");
			break;
	}
}

void SceneManager::UpdatePendingLoads(Application& app) {
	for (auto it{ pending_loads_.begin() }; it != pending_loads_.end();) {
		const auto progress{ it->ticket.GetProgress() };
		if (!progress.IsComplete()) {
			++it;
			continue;
		}

		if (progress.failed_assets > 0) {
			PTGN_WARN(
				"Failed to load ",
				progress.failed_assets,
				" asset(s) for scene: ",
				it->command.to_scene_tag,
				". Continuing with available assets."
			);
		}

		auto command{ std::move(it->command) };
		auto ticket{ std::move(it->ticket) };
		it = pending_loads_.erase(it);
		ApplyLoadedCommand(app, std::move(command), std::move(ticket));
	}
}

void SceneManager::ApplyCommands(
	Application& app,
	std::unordered_map<std::size_t, Command>& top_priority_commands
) {
	auto& assets{ ApplicationAccessor::ctx(app).assets };
	bool defer_exits{ !pending_loads_.empty() };

	for (auto it{ top_priority_commands.begin() }; it != top_priority_commands.end();) {
		auto& command{ it->second };
		if (command.type == CommandType::Exit) {
			++it;
			continue;
		}

		auto dependencies{ command.scene_factory.GetPreloadDependencies(app) };
		auto ticket{ assets.AcquireDependenciesAsync(dependencies) };
		const auto progress{ ticket.GetProgress() };

		if (!progress.IsComplete()) {
			pending_loads_.emplace_back(PendingLoad{
				.command = std::move(command),
				.ticket = std::move(ticket),
			});
			defer_exits = true;
		} else {
			if (progress.failed_assets > 0) {
				PTGN_WARN(
					"Failed to load ",
					progress.failed_assets,
					" asset(s) for scene: ",
					command.to_scene_tag,
					". Continuing with available assets."
				);
			}
			ApplyLoadedCommand(app, std::move(command), std::move(ticket));
		}

		it = top_priority_commands.erase(it);
	}

	for (auto& [target_scene_tag_hash, command] : top_priority_commands) {
		PTGN_ASSERT(command.type == CommandType::Exit, "Only exit commands should remain");

		if (defer_exits) {
			commands_.emplace_back(std::move(command));
			continue;
		}

		PTGN_ASSERT(
			HasScene(target_scene_tag_hash),
			"Cannot exit a scene which is not in the scene manager"
		);

		auto& target_scene{ GetScene(target_scene_tag_hash) };
		target_scene.data_.state = SceneState::TransitionOut;
		target_scene.data_.transition = std::move(command.transition_out);
		if (target_scene.data_.transition) {
			target_scene.data_.transition->OnDelayStart(target_scene);
			if (!target_scene.data_.transition->IsInDelay()) {
				target_scene.data_.transition->started_ = true;
				target_scene.data_.transition->OnStart(target_scene);
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

	UpdatePendingLoads(app);

	auto top_priority_commands{ GetTopPriorityCommands() };
	ApplyCommands(app, top_priority_commands);
	UpdateTransitions(dt);
	UpdateReEnteredSceneTagHashes();
}

void SceneManager::UpdateTransitions(secondsf dt) {
	for (auto it{ scenes_.begin() }; it != scenes_.end();) {
		using enum SceneState;

		const auto& scene{ *it };
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
	for (auto it{ reentering_scenes_.begin() }; it != reentering_scenes_.end();) {
		if (!HasScene(it->temporary_scene_tag_hash)) {
			it = reentering_scenes_.erase(it);
			continue;
		}

		auto& scene{ GetScene(it->temporary_scene_tag_hash) };
		if (scene.data_.state == SceneState::Active) {
			scene.data_.tag_hash = it->scene_tag_hash;
			it = reentering_scenes_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneManager::Draw(DrawContext& ctx) const {
	for (const auto& scene : scenes_) {
		if (scene->IsAwaitingTransitionDelay()) {
			continue;
		}
		scene->InternalDraw(ctx);
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
	auto it{ std::ranges::find_if(scenes_, [scene_tag_hash](const auto& scene) {
		return scene->data_.tag_hash == scene_tag_hash;
	}) };
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
	if (std::ranges::any_of(pending_loads_, [target_scene_tag_hash](const auto& pending) {
			return pending.command.to_scene_tag_hash == target_scene_tag_hash;
		})) {
		return false;
	}

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
	scene_manager_{ scene_manager }, scene_{ &scene } {}

void LocalSceneManager::Rebind(Scene& scene) {
	scene_ = &scene;
}

bool LocalSceneManager::CanIssueCommands() const {
	if (!scene_ ||
		scene_->IsTransitioning()) {
		return false;
	}

	for (const auto& active_scene :
		 scene_manager_.GetScenes()) {
		if (active_scene.get() ==
			scene_) {
			return true;
		}
	}

	return false;
}

} // namespace ptgn
