#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>
#include <vector>

#include "app/application_config.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

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

		ctx_.state = ApplicationState::Running;
		auto& scene{ ctx_.scene_manager.scenes_.emplace_back(std::move(first_scene)) };
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
		requires std::constructible_from<T, TArgs...> && std::derived_from<T, ApplicationLayer>
	void PushLayer(TArgs&&... args) {
		ctx_.layers.emplace_back(std::make_unique<T>(std::forward<TArgs>(args)...));
	}

private:
	friend class impl::ApplicationAccessor;

	[[nodiscard]] constexpr bool Can(impl::ApplicationFeature feature) const {
		auto features{ impl::ApplicationFeaturesForState(ctx_.state) };
		return impl::ApplicationHasFeature(features, feature);
	}

	void EnterMainLoop();
	void Update();
	void HandleGlobalEvents(bool dispatch_scene_events);

	impl::ApplicationContext ctx_;
};

} // namespace ptgn