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

	Window& window;
	Renderer& renderer;
	SceneManager& scenes;
	EventHandler& events;
	InputHandler& input;
	AssetManager& assets;
	FontSystem& font;
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