#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "app/layer.h"
#include "core/event/event_handler.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_manager.h"
#include "serialization/serialize.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;
class SceneContext;

namespace editor {

// TODO: Remove eventually.
class Editor;

} // namespace editor

namespace impl {

class ApplicationLibrary {
private:
	friend class ptgn::Application;

	ApplicationLibrary();
	~ApplicationLibrary() noexcept;
	ApplicationLibrary(const ApplicationLibrary&)				 = delete;
	ApplicationLibrary& operator=(const ApplicationLibrary&)	 = delete;
	ApplicationLibrary(ApplicationLibrary&&) noexcept			 = delete;
	ApplicationLibrary& operator=(ApplicationLibrary&&) noexcept = delete;
};

} // namespace impl

/// @brief Configuration data used to initialize an Application.
struct ApplicationConfig {
	WindowConfig window;
	PTGN_SERIALIZE(ApplicationConfig, window)
};

/// @brief Core engine entry point coordinating windowing, rendering,
///        input, scenes, assets, and the main loop.
///
/// Owns all major subsystems and drives the frame update loop.
class Application {
public:
	/// @brief Constructs the application using the provided configuration.
	/// @param config Application initialization settings (window, etc.).
	explicit Application(const ApplicationConfig& config = {});
	explicit Application(std::string_view title);
	explicit Application(std::string_view title, V2_int window_size);

	~Application() noexcept;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	/// @brief Starts the application with the specified initial scene.
	///
	/// @tparam TScene Scene type to instantiate.
	/// @param scene_tag Unique name for the scene instance.
	/// @param args Arguments forwarded to the scene constructor.
	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_tag, TArgs&&... args) {
		auto first_scene = std::make_unique<TScene>(std::forward<TArgs>(args)...);

		auto& scene{ scene_manager_.scenes_.emplace_back(std::move(first_scene)) };
		scene->Init(
			*this, impl::SceneData{ .tag{ scene_tag },
									.tag_hash{ Hash(scene_tag) },
									.state{ impl::SceneState::Active } }
		);
		scene->InternalEnter();

		EnterMainLoop();
	}

	template <SceneType TScene>
		requires std::is_default_constructible_v<TScene>
	void StartWith() {
		StartWith<TScene>("");
	}

	template <typename T, typename... TArgs>
		requires std::constructible_from<T, TArgs...> && std::derived_from<T, Layer>
	void PushLayer(TArgs&&... args) {
		layers_.emplace_back(std::make_unique<T>(std::forward<TArgs>(args)...));
	}

private:
	friend class SceneContext;
	// TODO: Remove eventually.
	friend class editor::Editor;

	impl::ApplicationLibrary app_library_;

	EventHandler event_handler_;

	// Must be created before every other system and hence destroyed after every other system.
	Window window_;

	// Must be created after Window but before other systems that rely on it.
	impl::Renderer renderer_;

	impl::SceneManager scene_manager_;
	AssetManager assets_;
	FontSystem font_;
	AudioSystem audio_;

	DebugSystem debug_;

	void EnterMainLoop();
	void Update();
	void HandleGlobalEvents();

	[[nodiscard]] milliseconds TimeSinceStart() const;

	void Stop();

	[[nodiscard]] secondsf dt() const;

	[[nodiscard]] bool IsRunning() const;

	std::size_t GetFrameCount() const;

	secondsf dt_{ 0.0f };
	bool running_{ false };
	std::size_t frame_count_{ 0 };

	std::vector<std::unique_ptr<Layer>> layers_;
};

} // namespace ptgn