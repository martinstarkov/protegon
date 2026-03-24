#pragma once

#include <concepts>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_context.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_command.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Renderer;
class RenderTarget;
class Application;
class FontSystem;
class AssetManager;
class Scene;
class EventHandler;
class Window;
class AudioSystem;

class SceneEventHandler {
public:
	explicit SceneEventHandler(Scene& scene);

	void Emit(EventDispatcher d);

private:
	Scene& scene_;
};

class LocalSceneManager {
public:
	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void Enter(
		std::string_view scene_key, std::unique_ptr<SceneTransition> transition_in,
		std::size_t priority, TArgs&&... constructor_args
	);

	void Exit(
		std::string_view scene_key, std::unique_ptr<SceneTransition> transition_out,
		std::size_t priority
	);

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void ReEnter(
		std::string_view scene_key, std::unique_ptr<SceneTransition> transition_in,
		std::unique_ptr<SceneTransition> transition_out, TArgs&&... constructor_args
	);

private:
	friend class SceneContext;

	explicit LocalSceneManager(SceneManager& scene_manager, Scene& scene);

	[[nodiscard]] bool CanIssueCommands(std::size_t target_key) const;

	SceneManager& scene_manager_;
	Scene& scene_;
};

class SceneContext {
public:
	SceneContext() = delete;
	explicit SceneContext(Application& app, Scene& parent_scene);
	~SceneContext() noexcept;
	SceneContext(const SceneContext&)				 = delete;
	SceneContext& operator=(const SceneContext&)	 = delete;
	SceneContext(SceneContext&&) noexcept			 = default;
	SceneContext& operator=(SceneContext&&) noexcept = default;

	EventHandler& global_event;
	Window& window;
	AssetManager& asset;
	FontSystem& font;
	AudioSystem& audio;

	LocalSceneManager scene;
	RenderContext renderer;
	DebugContext debug;
	SceneEventHandler event;
	SceneInput input;
	Physics physics;
	CollisionHandler collision;

	/// @brief The default camera used by all objects in the scene. By default it resizes to the
	/// game size.
	Camera camera;

	/// @brief Terminates the main application loop.
	void Stop();

	/// @brief Returns the delta time of the current frame.
	secondsf dt() const;

	/// @brief Returns the time elapsed since the Application instance was constructed.
	[[nodiscard]] milliseconds TimeSinceStart() const;

	/// @brief Returns whether the application is currently running.
	bool IsRunning() const;

	/// @brief Returns the total number of frames that the application has run for.
	std::size_t GetFrameCount() const;

private:
	friend class RenderTarget;
	friend class Scene;
	friend class LocalSceneManager;

	/// @brief An optional secondary fixed camera for the scene. By default it resizes to the game
	/// size.
	Camera fixed_camera_;

	Renderer& global_renderer_;
	Application& app_;
};

template <SceneType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void LocalSceneManager::Enter(
	std::string_view key, std::unique_ptr<SceneTransition> transition_in, std::size_t priority,
	TArgs&&... constructor_args
) {
	auto key_hash{ Hash(key) };

	if (!CanIssueCommands(key_hash)) {
		return;
	}

	if (scene_manager_.Has(key_hash)) {
		return;
	}

	scene_manager_.commands_.emplace_back(
		impl::SceneCommandType::Enter, scene_.key_, key_hash, priority,
		[constructor_args...]() -> std::unique_ptr<Scene> {
			auto scene{ std::make_unique<T>(constructor_args) };
			scene->Init(scene_.ctx().app_);
			return scene;
		},
		transition_in, nullptr
	);
}

template <SceneType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void LocalSceneManager::ReEnter(
	std::string_view key, std::unique_ptr<SceneTransition> transition_in,
	std::unique_ptr<SceneTransition> transition_out, TArgs&&... constructor_args
) {
	auto key_hash{ Hash(key) };

	if (!CanIssueCommands(key_hash)) {
		return;
	}

	PTGN_ASSERT(
		scene_manager_.Has(key_hash), "Cannot re-enter a scene key which has not been entered"
	);

	scene_manager_.commands_.emplace_back(
		impl::SceneCommandType::ReEnter, scene_.key_, key_hash,
		std::numeric_limits<std::size_t>::infinity(),
		[constructor_args...]() -> std::unique_ptr<Scene> {
			auto scene{ std::make_unique<T>(constructor_args) };
			scene->Init(scene_.ctx().app_);
			return scene;
		},
		std::move(transition_in), std::move(transition_out)
	);
}

} // namespace ptgn