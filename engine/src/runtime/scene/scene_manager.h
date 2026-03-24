#pragma once
#include <memory>
#include <vector>

namespace ptgn {

class Application;
class LocalSceneManager;
class Scene;
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

	void Update();

	std::vector<std::unique_ptr<Scene>> scenes_;
};

class LocalSceneManager {
public:
	// TODO: Add Enter, Exit, ReEnter.

private:
	friend class SceneContext;

	explicit LocalSceneManager(SceneManager& scene_manager);

	SceneManager& scene_manager_;
};

} // namespace ptgn