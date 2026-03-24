#pragma once

#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_context.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
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

class SceneContext {
public:
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

	explicit SceneContext(Application& app, Scene& scene);

	/// @brief An optional secondary fixed camera for the scene. By default it resizes to the game
	/// size.
	Camera fixed_camera_;

	Renderer& global_renderer_;
};

} // namespace ptgn