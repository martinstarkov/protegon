#pragma once

#include <concepts>
#include <filesystem>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_config.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"

namespace ptgn {

/// @brief Core engine entry point coordinating windowing, rendering, input, scenes, assets, and the
/// main loop.
class Application {
public:
	explicit Application(const ApplicationConfig& config = {});
	explicit Application(std::string_view title);
	explicit Application(std::string_view title, V2_int window_size);

	~Application() noexcept;
	Application(const Application&)            = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) noexcept         = delete;
	Application& operator=(Application&&) noexcept = delete;

	/// @brief Opens an existing project. The project file must already exist.
	void StartProject(const path& project_path);

	/// @brief Opens a project or creates it with TDefaultScene when it does not exist.
	template <SceneType TDefaultScene>
		requires std::default_initializable<TDefaultScene>
	void StartProject(const path& project_path) {
		const impl::SceneRegistryEntry* default_scene{ nullptr };
		if (!FileExists(project_path)) {
			default_scene = &impl::GetSceneRegistration<TDefaultScene>();
		}
		StartProjectImpl(project_path, default_scene);
	}

	/// @brief Starts a code only runtime scene without creating or loading a project.
	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_tag, TArgs&&... args) {
		impl::SceneFactory factory = [args...] (
			Application& app, impl::SceneData&& scene_data
		) mutable -> std::unique_ptr<Scene> {
			auto scene{ std::make_unique<TScene>(args...) };
			scene_data.runtime = true;
			scene->Init(app, std::move(scene_data));
			return scene;
		};

		StartWithFactory(scene_tag, std::move(factory));
	}

	template <SceneType TScene>
		requires std::default_initializable<TScene>
	void StartWith() {
		StartWith<TScene>("");
	}

	template <typename T, typename... TArgs>
		requires std::constructible_from<T, TArgs...> && std::derived_from<T, ApplicationLayer>
	T& PushLayer(TArgs&&... args) {
		return static_cast<T&>(
			*ctx_.layers.emplace_back(std::make_unique<T>(std::forward<TArgs>(args)...))
		);
	}

private:
	friend class impl::ApplicationAccessor;

	[[nodiscard]] constexpr bool Can(impl::ApplicationFeature feature) const {
		auto features{ impl::ApplicationFeaturesForState(ctx_.state) };
		return impl::ApplicationHasFeature(features, feature);
	}

	void StartProjectImpl(
		const path& project_path,
		const impl::SceneRegistryEntry* default_scene
	);
	void StartWithFactory(std::string_view scene_tag, impl::SceneFactory scene_factory);
	void EnterMainLoop();
	void Update();
	void RenderScenes();
	void HandleGlobalEvents(bool dispatch_scene_events);

	impl::ApplicationContext ctx_;
};

} // namespace ptgn
