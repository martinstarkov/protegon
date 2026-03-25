#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/time/time.h"
#include "runtime/scene/scene_command.h"

namespace ptgn {

class Application;
class Scene;
class LocalSceneManager;
class EventHandler;

class SceneManager {
private:
	friend class Application;
	friend class LocalSceneManager;
	friend class EventHandler;

	SceneManager()									 = default;
	~SceneManager() noexcept						 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	/// @return A map of scene keys to the highest priority command for each scene (if a command was
	/// issued).
	std::unordered_map<std::size_t, impl::SceneCommand> GetTopPriorityCommands();

	void ApplyCommands(std::unordered_map<std::size_t, impl::SceneCommand>& top_priority_commands);

	void Update(secondsf dt);

	[[nodiscard]] std::size_t GenerateTempKey() const;

	[[nodiscard]] bool Has(std::size_t key) const;
	const Scene& Get(std::size_t key) const;
	Scene& Get(std::size_t key);

	std::vector<std::unique_ptr<Scene>> scenes_;
	std::vector<impl::SceneCommand> commands_;
	/// @brief Contains the scene key of currently re-entering scenes.
	std::vector<impl::ReEnteringScene> reentering_scenes_;
};

} // namespace ptgn