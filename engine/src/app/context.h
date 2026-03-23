#pragma once

#include "core/time/time.h"

namespace ptgn {

class Application;
class Window;
class Renderer;
class SceneManager;
class EventHandler;
class InputHandler;
class AssetManager;
class FontSystem;
class AudioSystem;

/// @brief Provides controlled access to core subsystems of an Application.
class ApplicationContext {
private:
	Application& app_;

public:
	explicit ApplicationContext(Application& app);

	// TODO: Move into scene context.
	Window& window;
	// TODO: Move into scene context.
	Renderer& renderer;

	// TODO: Make this be hidden part of application. Use LocalSceneManager instead.
	SceneManager& scene;

	// TODO: Make this be called global event handler or something.
	EventHandler& event;

	// TODO: Remove in favor of scene input.
	InputHandler& input;

	// TODO: Move into scene context.
	AssetManager& asset;

	// TODO: Move into scene context.
	FontSystem& font;

	// TODO: Move into scene context.
	AudioSystem& audio;

	/// @brief Terminates the main application loop.
	void Stop();

	/// @brief Returns the delta time of the current frame.
	secondsf DeltaTime() const;

	/// @brief Returns the time elapsed since the Application instance was constructed.
	[[nodiscard]] milliseconds TimeSinceStart() const;

	/// @brief Returns whether the application is currently running.
	bool IsRunning() const;

	/// @brief Returns the total number of frames that the application has run for.
	std::size_t GetFrameCount() const;
};

} // namespace ptgn