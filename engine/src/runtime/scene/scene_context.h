#pragma once

#include "core/util/time.h"
#include "runtime/graphics/render_context.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_event_handler.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;
class FontSystem;
class AssetManager;
class Window;
class AudioSystem;
class RenderTarget;
class LocalSceneManager;

namespace impl {

class Renderer;

} // namespace impl

class SceneContext {
public:
	SceneContext() = delete;
	explicit SceneContext(Application& app, Scene& parent_scene);
	~SceneContext() noexcept;
	SceneContext(const SceneContext&)				 = delete;
	SceneContext& operator=(const SceneContext&)	 = delete;
	SceneContext(SceneContext&&) noexcept			 = default;
	SceneContext& operator=(SceneContext&&) noexcept = delete;

	Window& window;
	AssetManager& asset;
	FontSystem& font;
	AudioSystem& audio;

	LocalSceneManager scene;
	RenderContext renderer;
	DebugContext debug;
	LocalEventHandler event;
	SceneInput input;
	InteractionSystem interaction;
	Physics physics;
	CollisionHandler collision;

	/// @brief The default camera used by all objects in the scene. By default it resizes to the
	/// game size.
	SceneCamera camera;

	/// @brief Terminates the main application loop.
	void Stop();

	/// @brief Returns the delta time of the current frame.
	secondsf dt() const;

	/// @brief Returns the delta time of the current frame.
	template <DurationType T>
	[[nodiscard]] T dt() const {
		return duration_cast<T>(dt());
	}

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
	SceneCamera fixed_camera_;

	impl::Renderer& global_renderer_;

	Application& app_;
};

} // namespace ptgn