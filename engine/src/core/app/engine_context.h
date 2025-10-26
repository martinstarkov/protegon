#pragma once

#include <string>

#include "core/app/window.h"
#include "core/events/event_handler.h"
#include "core/input/input_handler.h"
#include "core/resource/asset_manager.h"
#include "debug/runtime/debug_system.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "world/scene/scene_manager.h"

namespace ptgn {

struct ApplicationConfig {
	std::string title{ "Default Title" };
	V2_int window_size{ 800, 800 };
};

class Application {
public:
	Application(const ApplicationConfig& config = {});
	~Application();
	Application(const Application&)			   = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&)				   = delete;
	Application& operator=(Application&&)	   = delete;

private:
	Window window_;
	Renderer render_;
	InputHandler input_;
	AssetManager assets_;
	SceneManager scenes_;
	EventRegistry events_;
	DebugSystem debug_;

	float dt_{ 0.0f };
	bool running_{ false };
};

} // namespace ptgn