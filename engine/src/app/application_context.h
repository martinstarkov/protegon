#pragma once

#include <memory>
#include <vector>

#include "app/application_config.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "core/event/event_handler.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;

namespace impl {

class ApplicationContext;

class ApplicationLibrary {
private:
	friend class ApplicationContext;

	ApplicationLibrary();
	~ApplicationLibrary() noexcept;
	ApplicationLibrary(const ApplicationLibrary&)				 = delete;
	ApplicationLibrary& operator=(const ApplicationLibrary&)	 = delete;
	ApplicationLibrary(ApplicationLibrary&&) noexcept			 = delete;
	ApplicationLibrary& operator=(ApplicationLibrary&&) noexcept = delete;
};

class ApplicationAccessor {
public:
	[[nodiscard]] static ApplicationContext& ctx(Application& app);
	[[nodiscard]] static const ApplicationContext& ctx(const Application& app);
};

class ApplicationContext {
public:
	ApplicationLibrary app_library;
	EventHandler event_handler;
	Window window;
	Renderer renderer;
	SceneManager scene_manager;
	AssetManager assets;
	FontSystem font;
	AudioSystem audio;

	DebugSystem debug;
	float fps{ 60.0f };
	secondsf dt{ 0.0f };
	bool running{ false };
	std::size_t frame_count{ 0 };
	float time_scale{ 1.0f };
	bool step_requested{ false };

	std::vector<std::unique_ptr<ApplicationLayer>> layers;

	ApplicationState state{ ApplicationState::Launching };

	[[nodiscard]] milliseconds TimeSinceStart() const;

private:
	friend class ptgn::Application;

	ApplicationContext() = delete;
	explicit ApplicationContext(const ApplicationConfig& config);
	~ApplicationContext() noexcept;
	ApplicationContext(const ApplicationContext&)				 = delete;
	ApplicationContext& operator=(const ApplicationContext&)	 = delete;
	ApplicationContext(ApplicationContext&&) noexcept			 = delete;
	ApplicationContext& operator=(ApplicationContext&&) noexcept = delete;
};

} // namespace impl

} // namespace ptgn