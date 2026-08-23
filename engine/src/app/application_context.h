#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/application_config.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "app/project.h"
#include "core/event/event_handler.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;

namespace impl {

class ApplicationContext;

struct RuntimeProjectSceneSnapshot {
	std::string key{};
	SerializedScene scene{};
};

class ApplicationLibrary {
private:
	friend class ApplicationContext;

	ApplicationLibrary();
	~ApplicationLibrary() noexcept;
	ApplicationLibrary(const ApplicationLibrary&)            = delete;
	ApplicationLibrary& operator=(const ApplicationLibrary&) = delete;
	ApplicationLibrary(ApplicationLibrary&&) noexcept         = delete;
	ApplicationLibrary& operator=(ApplicationLibrary&&) noexcept = delete;
};

class ApplicationAccessor {
public:
	[[nodiscard]] static ApplicationContext& ctx(Application& app);
	[[nodiscard]] static const ApplicationContext& ctx(const Application& app);
};

class ApplicationContext {
public:
	DebugSystem debug;
	ApplicationLibrary app_library;
	EventHandler event_handler;
	Window window;
	Renderer renderer;
	SceneManager scene_manager;
	AssetManager assets;
	FontSystem font;
	AudioSystem audio;
	Manager screen_effect_manager;
	std::vector<Entity> screen_effect_order;
	bool screen_effects_enabled{ true };
	std::uint64_t next_screen_effect_runtime_id{ 1 };

	float fps{ 60.0f };
	secondsf dt{ 0.0f };
	bool running{ false };
	std::size_t frame_count{ 0 };
	float time_scale{ 1.0f };
	bool step_requested{ false };

	std::vector<std::unique_ptr<ApplicationLayer>> layers;

	ApplicationState state{ ApplicationState::Launching };

	/// @brief Runtime mode used when StartProject loads the startup scene.
	/// Generic engine state; the editor sets this to false before project startup.
	bool start_project_runtime{ true };

	bool project_bootstrap_save_pending{ false };

	std::optional<Project> project;

	/// @brief In-memory editor play snapshots used by project scene transitions.
	/// Empty for direct runtime projects, which load scene files from disk.
	std::vector<RuntimeProjectSceneSnapshot> runtime_project_scenes;

	[[nodiscard]] milliseconds TimeSinceStart() const;

private:
	friend class ptgn::Application;

	ApplicationContext() = delete;
	explicit ApplicationContext(const ApplicationConfig& config);
	~ApplicationContext() noexcept;
	ApplicationContext(const ApplicationContext&)            = delete;
	ApplicationContext& operator=(const ApplicationContext&) = delete;
	ApplicationContext(ApplicationContext&&) noexcept         = delete;
	ApplicationContext& operator=(ApplicationContext&&) noexcept = delete;
};

} // namespace impl

} // namespace ptgn
