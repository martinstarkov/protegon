#pragma once

namespace ptgn {

class Application;
class LocalSceneManager;

class SceneManager {
private:
	friend class Application;
	friend class LocalSceneManager;

	SceneManager()									 = default;
	~SceneManager() noexcept						 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	void Update();
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