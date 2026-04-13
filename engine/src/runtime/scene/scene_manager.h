#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/time/time.h"

namespace ptgn {

class Scene;
class SceneTransition;

struct SceneTransitionPriority {
	explicit SceneTransitionPriority() = default;

	/// @brief Explicit construction prevents conflict with scene constructor args.
	explicit SceneTransitionPriority(std::size_t value) : value{ value } {}

	std::size_t value{ 0 };
};

namespace impl {

class SceneManager {
public:
	enum class CommandType {
		Enter,
		Exit,
		ReEnter
	};

	struct Command {
		CommandType type{ CommandType::Enter };

		std::size_t from_scene_key{ 0 };
		std::size_t to_scene_key{ 0 };

		SceneTransitionPriority priority;

		std::function<std::unique_ptr<Scene>()> scene_factory;

		std::unique_ptr<SceneTransition> transition_out;
		std::unique_ptr<SceneTransition> transition_in;
	};

	SceneManager()									 = default;
	~SceneManager() noexcept						 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	void Update(secondsf dt);
	void Draw() const;

	template <typename... TArgs>
		requires std::constructible_from<Command, TArgs...>
	void PushCommand(TArgs&&... args) {
		commands_.emplace_back(std::forward<TArgs>(args)...);
	}

	[[nodiscard]] bool HasScene(std::size_t scene_key) const;

	const std::vector<std::unique_ptr<Scene>>& GetScenes() const;

	const Scene& GetScene(std::size_t scene_key) const;
	Scene& GetScene(std::size_t scene_key);

private:
	struct ReEnteringScene {
		std::size_t scene_key{ 0 };
		std::size_t temporary_scene_key{ 0 };
	};

	/// @return A map of scene keys to the highest priority command for each scene (if a command was
	/// issued).
	std::unordered_map<std::size_t, Command> GetTopPriorityCommands();
	void ApplyCommands(std::unordered_map<std::size_t, Command>& top_priority_commands);
	void UpdateTransitions(secondsf dt);
	void UpdateReEnteredSceneKeys();

	[[nodiscard]] std::size_t GenerateTempKey() const;

	std::vector<std::unique_ptr<Scene>> scenes_;

	std::vector<Command> commands_;

	/// @brief Contains the scene key of currently re-entering scenes.
	std::vector<ReEnteringScene> reentering_scenes_;
};

} // namespace impl

} // namespace ptgn