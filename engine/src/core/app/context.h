#pragma once

#include "core/util/time.h"

namespace ptgn {

class Application;
class Window;
class Renderer;
class SceneManager;
class EventHandler;
class InputHandler;
class AssetManager;

class ApplicationContext {
private:
	Application& app_;

public:
	ApplicationContext(Application& app);

	Window& window;
	Renderer& renderer;
	SceneManager& scenes;
	EventHandler& events;
	InputHandler& input;
	AssetManager& assets;

	void Stop();

	secondsf Dt() const;

	[[nodiscard]] milliseconds Time() const;

	bool IsRunning() const;
};

} // namespace ptgn