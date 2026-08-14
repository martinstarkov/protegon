#include "app/application.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#endif

#include <ecs/ecs.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "app/application_config.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "app/project.h"
#include "core/assert.h"
#include "core/build_info.h"
#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/event/window_event.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "renderer/draw_context.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

namespace {

impl::AssetLoadTicket LoadStartupDependencies(
	Application& app,
	const impl::SceneFactory& scene_factory
) {
	auto& assets{ impl::ApplicationAccessor::ctx(app).assets };
	auto ticket{ assets.AcquireDependenciesAsync(
		scene_factory.GetPreloadDependencies(app)
	) };

	while (!ticket.IsComplete()) {
		assets.Update();
#ifndef __EMSCRIPTEN__
		std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
#endif
	}

	const auto progress{ ticket.GetProgress() };
	if (progress.failed_assets > 0) {
		PTGN_WARN(
			"Failed to load ",
			progress.failed_assets,
				" startup asset(s); continuing with available assets"
		);
	}

	return ticket;
}

void LoadProjectPreloads(Application& app) {
	auto& assets{ impl::ApplicationAccessor::ctx(app).assets };
	const auto& dependencies{ assets.GetProjectAssetDependencies() };
	if (dependencies.empty()) {
		return;
	}

	auto ticket{ assets.AcquireDependenciesAsync(dependencies) };
	while (!ticket.IsComplete()) {
		assets.Update();
#ifndef __EMSCRIPTEN__
		std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
#endif
	}

	const auto progress{ ticket.GetProgress() };
	if (progress.failed_assets > 0) {
		PTGN_WARN(
			"Failed to load ",
			progress.failed_assets,
				" project preload asset(s); continuing with available assets"
		);
	}
}

[[nodiscard]] path ResolveStartupProjectPath(
	const path& project_path
) {
	if (project_path.empty() || project_path.is_absolute()) {
		return project_path.lexically_normal();
	}

	const auto& build_info{ impl::GetBuildInfo() };
	if (build_info.distribution || build_info.web) {
		return (
			build_info.runtime_root / project_path
		).lexically_normal();
	}

	return project_path.lexically_normal();
}

} // namespace

Application::Application(const ApplicationConfig& config) : ctx_{ config } {}

Application::Application(std::string_view title) :
	Application{ ApplicationConfig{ .window{ .title{ title } } } } {}

Application::Application(std::string_view title, V2_int window_size) :
	Application{ ApplicationConfig{ .window{ .title{ title }, .size{ window_size } } } } {}

Application::~Application() noexcept {
#if defined(__EMSCRIPTEN__)
	return;
#endif
	if (!ctx_.project.has_value()) {
		return;
	}

	try {
		SaveProjectLocalState(
			ctx_.project.value(),
			GetProjectLocalState(*this)
		);
	} catch (const std::exception& exception) {
		PTGN_ERROR(
			"Failed to save project local state: ",
			exception.what()
		);
	} catch (...) {
		PTGN_ERROR("Failed to save project local state");
	}
}

void Application::StartProject(const path& project_path) {
	StartProjectImpl(project_path, nullptr);
}

void Application::StartProjectImpl(
	const path& project_path,
	const impl::SceneRegistryEntry* default_scene
) {
	const path resolved_project_path{
		ResolveStartupProjectPath(project_path)
	};

	// Existing projects created before ProjectSettings was serialized inherit the
	// application's current configuration for any missing settings fields.
	const auto application_defaults{ GetProjectSettings(*this) };

	Project project;

	if (FileExists(resolved_project_path)) {
		project = LoadProject(
			resolved_project_path,
			application_defaults
		);
	} else {
#if defined(__EMSCRIPTEN__)
		PTGN_ASSERT(
			false,
			"Project does not exist in the Web runtime filesystem: ",
			resolved_project_path.string()
		);
#else
		PTGN_ASSERT(
			default_scene,
			"Project does not exist. Use "
			"StartProject<TDefaultScene>() to create it: ",
			resolved_project_path.string()
		);

		project = CreateProject(
			resolved_project_path,
			*default_scene,
			application_defaults
		);
#endif
	}

	ctx_.project = std::move(project);

	auto& loaded_project{ ctx_.project.value() };

	// Tracked project defaults are applied first. The ignored .ptgnlocal file then
	// restores this user's window geometry without mutating the tracked defaults.
	SetProjectSettings(
		*this,
		loaded_project.settings
	);
	SetProjectLocalState(
		*this,
		LoadProjectLocalState(loaded_project)
	);

	ctx_.assets.RegisterCatalog(loaded_project.assets, loaded_project);
	LoadProjectPreloads(*this);

	if (ctx_.start_project_runtime) {
		const auto& startup{
			GetStartupProjectScene(loaded_project)
		};

		auto serialized_scene{
			LoadSceneFile(
				GetProjectScenePath(
					loaded_project,
					startup
				)
			)
		};

		auto scene_factory{
			impl::MakeSceneFactory(
				std::move(serialized_scene),
				true
			)
		};

		StartWithFactory(
			startup.key,
			std::move(scene_factory)
		);

		return;
	}

	PTGN_ASSERT(
		ctx_.scene_manager.scenes_.empty(),
		"Application has already been started"
	);


	struct BootstrapScene {
		const ProjectSceneEntry* entry{ nullptr };
		Scene* scene{ nullptr };
	};

	std::vector<BootstrapScene> bootstrap_scenes;

	for (std::size_t i{ 0 };
		 i < loaded_project.scenes.size();
		 ++i) {
		const auto& entry{ loaded_project.scenes[i] };

		auto serialized_scene{
			LoadSceneFile(
				GetProjectScenePath(
					loaded_project,
					entry
				)
			)
		};

		bool requires_bootstrap_save{
			!serialized_scene.content.has_value()
		};

		auto scene_factory{
			impl::MakeSceneFactory(
				std::move(serialized_scene),
				false
			)
		};

		auto asset_ticket{ LoadStartupDependencies(*this, scene_factory) };

		auto scene{ scene_factory(
			*this,
			impl::SceneData{
				.tag = entry.key,
				.tag_hash = Hash(entry.key),
				.state = impl::SceneState::Active,
				.runtime = false,
				.first_scene = i == 0,
			}
		) };

		PTGN_ASSERT(
			scene,
			"Project scene factory returned null: ",
			entry.key
		);

		scene->AdoptLoadedAssetDependencies(asset_ticket.ReleaseOwnership());

		auto* scene_ptr{ scene.get() };

		ctx_.scene_manager.scenes_.emplace_back(
			std::move(scene)
		);

		if (requires_bootstrap_save) {
			bootstrap_scenes.emplace_back(
				BootstrapScene{
					.entry = &entry,
					.scene = scene_ptr,
				}
			);
		}
	}

	if (!bootstrap_scenes.empty()) {
		loaded_project.assets =
			ctx_.assets.GetCatalog();

		loaded_project.preload_assets =
			ctx_.assets.GetProjectAssetDependencies();

		loaded_project.settings =
			GetProjectSettings(*this);

		// Save catalog entries and settings before scene dependency keys.
		SaveProject(loaded_project);

		for (const auto& bootstrap :
			 bootstrap_scenes) {
			PTGN_ASSERT(
				bootstrap.entry &&
					bootstrap.scene
			);

			SaveSceneFile(
				GetProjectScenePath(
					loaded_project,
					*bootstrap.entry
				),
				CaptureScene(
					*bootstrap.scene
				)
			);
		}
	}

	ctx_.state = ApplicationState::Running;

	EnterMainLoop();
}

void Application::StartWithFactory(
	std::string_view scene_tag,
	impl::SceneFactory scene_factory
) {
	PTGN_ASSERT(scene_factory, "Cannot start application with a null scene factory");
	PTGN_ASSERT(ctx_.scene_manager.scenes_.empty(), "Application has already been started");

	auto asset_ticket{ LoadStartupDependencies(*this, scene_factory) };
	auto first_scene{ scene_factory(
		*this,
		impl::SceneData{
			.tag{ scene_tag },
			.tag_hash = Hash(scene_tag),
			.state = impl::SceneState::Active,
			.first_scene = true,
		}
	) };

	PTGN_ASSERT(first_scene, "Startup scene factory returned null");
	first_scene->AdoptLoadedAssetDependencies(asset_ticket.ReleaseOwnership());

	auto& scene{ ctx_.scene_manager.scenes_.emplace_back(std::move(first_scene)) };
	scene->InternalEnter();

	ctx_.state = ApplicationState::Running;
	EnterMainLoop();
}

void Application::EnterMainLoop() {
	// Only show window after initialization has completed.
	ctx_.window.SetSetting(WindowSetting::Shown);

	ctx_.running = true;

	ctx_.renderer.UpdateDisplayViewport(true);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(
		[](void* application) {
			auto& app{ *static_cast<Application*>(application) };

			app.Update();

			if (!app.ctx_.running) {
				emscripten_cancel_main_loop();
			}
		},
		this, /*fps=*/0, /*simulateInfiniteLoop=*/true
	);
#else
	while (ctx_.running) {
		Update();
	}
#endif
}

void Application::HandleGlobalEvents(bool dispatch_scene_events) {
	auto global_events{ std::exchange(ctx_.event_handler.global_event_queue_, {}) };

	for (auto& global_event : global_events) {
		Event event{ global_event };
		event.Dispatch<event::WindowResized>([this](const auto& size) {
			ctx_.renderer.OnOutputResize(size);
		});
		if (!dispatch_scene_events) {
			continue;
		}
		for (const auto& scene : ctx_.scene_manager.GetScenes()) {
			if (!scene->IsRuntime() || scene->IsAwaitingTransitionDelay()) {
				continue;
			}
			scene->InternalOnEvent(event);
		}
	}
}

void Application::RenderScenes() {
	ctx_.renderer.BeginFrame();

	DrawContext draw_context{ ctx_.renderer };

	ctx_.scene_manager.Draw(draw_context);

	ctx_.screen_effect_manager.Refresh();

	if (ctx_.screen_effect_manager.IsEmpty()) {
		ctx_.renderer.EndFrame(nullptr);
		return;
	}

	ctx_.renderer.EndFrame([this](auto& draw_ctx) {
		for (auto entity : ctx_.screen_effect_manager.Entities()) {
			PTGN_ASSERT(
				!entity.Has<impl::HDREffectTag>() ||
					IsHDRFormat(
						ctx_.renderer.GetFormat(ctx_.renderer.presentation_framebuffer_).value()
					),
				"Presentation framebuffer must use HDR format if it has an HDR effect"
			);
			impl::InvokeDrawable(draw_ctx, entity);
		}
	});
}

void Application::SetCloseGuard(std::function<bool()> close_guard) {
	close_guard_ = std::move(close_guard);
}

void Application::RequestQuit() {
	ctx_.window.RequestQuit();
}

void Application::Update() {
	bool step_requested{ ctx_.step_requested };

	bool update_scenes{ Can(impl::ApplicationFeature::UpdateScenes) || step_requested };
	bool scene_events{ Can(impl::ApplicationFeature::DispatchSceneEvents) || step_requested };
	bool scene_rendering{ Can(impl::ApplicationFeature::RenderScenes) || step_requested };

	ctx_.step_requested = false;

	ctx_.debug.PreUpdate();

	static auto start{ std::chrono::steady_clock::now() };
	static auto end{ std::chrono::steady_clock::now() };
	ctx_.dt = end - start;

	secondsf max_dt{ 1.0f / ctx_.fps };

	if (ctx_.dt > max_dt) {
		ctx_.dt = max_dt;
	}

	PTGN_ASSERT(ctx_.time_scale >= 0.0f, "Time scale cannot be negative");

	if (step_requested) {
		ctx_.dt = max_dt * ctx_.time_scale;
	} else if (ctx_.state == ApplicationState::Paused) {
		ctx_.dt = 0s;
	} else {
		ctx_.dt *= ctx_.time_scale;
	}

	start = end;

	const bool window_running{ ctx_.window.Update() };
	if (!window_running && close_guard_ && !close_guard_()) {
		ctx_.window.CancelQuit();
		ctx_.running = true;
	} else {
		ctx_.running = window_running;
	}
	ctx_.assets.Update();

	if (ctx_.window.GetSetting(WindowSetting::Minimized)) {
		ctx_.audio.Update();
		ctx_.debug.PostRender();
		end = std::chrono::steady_clock::now();
		ctx_.frame_count++;
		return;
	}

	ctx_.renderer.UpdateDisplayViewport();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (update_scenes) {
		ctx_.scene_manager.PreUpdate();
	}

	HandleGlobalEvents(scene_events);

	if (scene_events) {
		ctx_.scene_manager.OnEvent();
	}
	if (update_scenes) {
		ctx_.scene_manager.Update(*this, ctx_.dt);
	}

	for (const auto& layer : ctx_.layers) {
		PTGN_ASSERT(layer);
		if (layer->update_enabled_) {
			layer->OnUpdate();
		}
	}

	ctx_.audio.Update();

	if (scene_rendering) {
		RenderScenes();
	}

	for (const auto& layer : ctx_.layers) {
		PTGN_ASSERT(layer);
		if (layer->render_enabled_) {
			layer->OnRender();
		}
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ctx_.window.SwapBuffers();

	ctx_.debug.PostRender();

	end = std::chrono::steady_clock::now();
	ctx_.frame_count++;
}

} // namespace ptgn
