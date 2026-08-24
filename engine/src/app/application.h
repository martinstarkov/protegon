#pragma once

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "app/application_config.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
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
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) noexcept = delete;
	Application& operator=(Application&&) noexcept = delete;

	/// @brief Installs an application shutdown guard. Return false to cancel the close.
	void SetCloseGuard(std::function<bool()> close_guard);
	void RequestQuit();

	void SetScreenEffects(const ScreenEffectSettings& settings);
	void SetScreenEffectsEnabled(bool enabled);
	[[nodiscard]] bool AreScreenEffectsEnabled() const;

	/// @brief Opens an existing project or creates an empty project when it does not exist.
	void StartProject(const path& project_path = {});

	/// @brief Opens an existing project or creates it with TDefaultScene as its initial scene when it
	/// does not exist.
	template <SceneType TDefaultScene>
		requires std::default_initializable<TDefaultScene>
	void StartProject(const path& project_path = {}) {
		StartProjectImpl(
			project_path,
			&impl::GetSceneRegistration<TDefaultScene>()
		);
	}

	/// @brief Starts a code only runtime scene without creating or loading a project.
	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_tag, TArgs&&... args) {
		auto arguments{
			std::make_shared<std::tuple<std::decay_t<TArgs>...>>(
				std::forward<TArgs>(args)...
			)
		};

		impl::SceneFactory::Construct construct = [arguments](
			Application& app,
			impl::SceneData&& scene_data
		) -> std::unique_ptr<Scene> {
			auto scene{ std::apply(
				[](const auto&... values) {
					return std::make_unique<TScene>(values...);
				},
				*arguments
			) };
			scene_data.runtime = true;
			scene_data.registered_type = impl::GetRegisteredSceneType<TScene>();
			scene->Init(app, std::move(scene_data));
			return scene;
		};

		impl::SceneFactory::Preload preload = [arguments](Application&) {
			auto scene{ std::apply(
				[](const auto&... values) {
					return TScene{ values... };
				},
				*arguments
			) };
			AssetPreloadContext context;
			scene.OnPreload(context);
			for (const auto& key : scene.GetExplicitAssetDependencies()) {
				context.Add(key);
			}
			return context.GetDependencies();
		};

		StartWithFactory(
			scene_tag,
			impl::SceneFactory{ std::move(construct), std::move(preload) }
		);
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

	std::function<bool()> close_guard_;

	impl::ApplicationContext ctx_;
};

} // namespace ptgn
