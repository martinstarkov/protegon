#pragma once

#include "core/util/time.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/text/text.h"
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
class Renderer;
class SceneContext;

namespace impl {

class SceneContextAccessor {
public:
	[[nodiscard]] static Application& app(SceneContext& ctx);
	[[nodiscard]] static const Application& app(const SceneContext& ctx);
	[[nodiscard]] static SceneCamera GetFixedCamera(const SceneContext& ctx);
};

} // namespace impl

class SceneContext {
public:
	SceneContext() = delete;
	explicit SceneContext(Application& app, Scene& parent_scene);
	~SceneContext() noexcept;
	SceneContext(const SceneContext&)				 = delete;
	SceneContext& operator=(const SceneContext&)	 = delete;
	SceneContext(SceneContext&&) noexcept			 = delete;
	SceneContext& operator=(SceneContext&&) noexcept = delete;

	Window& window;
	Renderer& renderer;
	AssetManager& asset;
	FontSystem& font;
	AudioSystem& audio;
	DebugSystem& debug;

	LocalSceneManager scene;
	RenderQueue render_queue;
	LocalEventHandler event;
	SceneInput input;
	InteractionSystem interaction;
	Physics physics;
	CollisionHandler collision;

	/// @brief The default camera used by all objects in the scene. By default it resizes to the
	/// logical size.
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

	/// @brief Returns the time elapsed since the Application instance was constructed.
	[[nodiscard]] secondsf TimeSinceStartSeconds() const;

	/// @brief Returns whether the application is currently running.
	bool IsRunning() const;

	/// @brief Returns the total number of frames that the application has run for.
	std::size_t GetFrameCount() const;

private:
	friend class RenderTarget;
	friend class Scene;
	friend class LocalSceneManager;
	friend class impl::SceneContextAccessor;

	void Rebind(Scene& parent_scene);

	/// @brief An optional secondary fixed camera for the scene. By default it resizes to the
	/// logical size. Used as the default camera for UI.
	SceneCamera fixed_camera_;

	/// @brief The default render target for the scene. By default it resizes to the logical size.
	RenderTarget render_target_;

	Application& app_;
};

} // namespace ptgn