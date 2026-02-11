#pragma once

#include <optional>

#include "app/scaling_mode.h"
#include "core/event/event.h"
#include "core/math/vector2.h"
#include "core/time/time.h"

namespace ptgn {

class Application;
class Window;
class Renderer;
class SceneManager;
class EventHandler;
class InputHandler;
class AssetManager;

struct PresentationResized : public Event<PresentationResized> {
	V2_int size;
};

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

	void Stop();

	secondsf DeltaTime() const;

	[[nodiscard]] milliseconds TimeSinceStart() const;

	bool IsRunning() const;
	std::size_t GetFrameCount() const;

	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	[[nodiscard]] V2_int GetGameSize() const;
};

} // namespace ptgn