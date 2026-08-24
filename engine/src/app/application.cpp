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
#include "platform/executable.h"
#include "renderer/draw_context.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

namespace {

constexpr std::string_view kProjectExtension{ ".ptgnproj" };

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

[[nodiscard]] std::vector<path> FindProjectFiles(
	const path& directory
) {
	std::vector<path> projects;

	std::error_code error;

	if (
		!fs::is_directory(directory, error) ||
		error
	) {
		return projects;
	}

	for (
		fs::directory_iterator it{
			directory,
			fs::directory_options::skip_permission_denied,
			error
		},
		end;
		!error && it != end;
		it.increment(error)
	) {
		std::error_code entry_error;

		if (
			!it->is_regular_file(entry_error) ||
			entry_error
		) {
			continue;
		}

		if (
			GetExtension(it->path()) !=
			kProjectExtension
		) {
			continue;
		}

		projects.emplace_back(
			it->path().lexically_normal()
		);
	}

	std::ranges::sort(
		projects,
		{},
		[](const path& project) {
			return project.generic_string();
		}
	);

	return projects;
}

[[nodiscard]] path ResolveStartupProjectPath(
	const path& project_path
) {
	const auto& build_info{
		impl::GetBuildInfo()
	};

	PTGN_ASSERT(
		!build_info.project_name.empty(),
		"CMake project name is empty"
	);

	path requested_path{
		project_path.empty()
			? path{ "." }
			: project_path
	};

	path resolved_path{
		requested_path.is_absolute()
			? requested_path.lexically_normal()
			: (
				GetRuntimeRoot() /
				requested_path
			).lexically_normal()
	};

	// An explicit .ptgnproj path always refers to that exact file,
	// whether or not the file exists yet.
	if (
		GetExtension(resolved_path) ==
		kProjectExtension
	) {
		return resolved_path;
	}

	// An existing regular file must be a project file.
	if (FileExists(resolved_path)) {
		PTGN_ASSERT(
			false,
			"Expected a .ptgnproj file: ",
			resolved_path.string()
		);
	}

	// Anything other than an explicit .ptgnproj file is treated
	// as a directory.
	//
	// Prefer:
	//   <directory>/<CMake project name>.ptgnproj
	path preferred_project{
		resolved_path /
		(
			build_info.project_name +
			std::string{ kProjectExtension }
		)
	};

	if (FileExists(preferred_project)) {
		return preferred_project;
	}

	// Otherwise use the lexicographically first project file.
	auto projects{
		FindProjectFiles(resolved_path)
	};

	if (!projects.empty()) {
		return projects.front();
	}

	// No project exists. Returning the preferred path allows
	// StartProject<TDefaultScene>() to create it.
	return preferred_project;
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

	const auto application_defaults{
		GetProjectSettings(*this)
	};

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
		project = CreateProject(
			resolved_project_path,
			default_scene,
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
	SetScreenEffects(loaded_project.screen_effects);
	SetProjectLocalState(
		*this,
		LoadProjectLocalState(loaded_project)
	);

	if (ctx_.assets.RegisterCatalog(loaded_project.assets, loaded_project)) {
		loaded_project.assets = ctx_.assets.GetCatalog();
#if !defined(__EMSCRIPTEN__)
		SaveProject(loaded_project);
#endif
	}
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
	impl::RefreshScreenEffectOrder(ctx_);

	if (!ctx_.screen_effects_enabled || ctx_.screen_effect_order.empty()) {
		ctx_.renderer.EndFrame(nullptr);
		return;
	}

	ctx_.renderer.EndFrame([this](auto& draw_ctx) {
		for (Entity entity : ctx_.screen_effect_order) {
			if (!entity || !IsVisible(entity)) {
				continue;
			}

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

void Application::SetScreenEffects(const ScreenEffectSettings& settings) {
	impl::RebuildScreenEffects(ctx_, settings);
}

void Application::SetScreenEffectsEnabled(bool enabled) {
	ctx_.screen_effects_enabled = enabled;
}

bool Application::AreScreenEffectsEnabled() const {
	return ctx_.screen_effects_enabled;
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

	static auto previous_time{ std::chrono::steady_clock::now() };

	auto current_time{ std::chrono::steady_clock::now() };
	secondsf real_dt{ current_time - previous_time };
	previous_time = current_time;

	secondsf unscaled_dt{ real_dt };

	secondsf max_dt{ 1.0f / ctx_.fps };

	if (unscaled_dt > max_dt || step_requested) {
		unscaled_dt = max_dt;
	}

	PTGN_ASSERT(ctx_.time_scale >= 0.0f, "Time scale cannot be negative");

	if (ctx_.state == ApplicationState::Paused && !step_requested) {
		unscaled_dt = 0s;
	}

	ctx_.dt = unscaled_dt * ctx_.time_scale;

	const bool window_running{ ctx_.window.Update() };
	if (!window_running && close_guard_ && !close_guard_()) {
		ctx_.window.CancelQuit();
		ctx_.running = true;
	} else {
		ctx_.running = window_running;
	}

	ctx_.assets.Update();

	auto end_frame = [&]() {
		ctx_.debug.PostRender();

		ctx_.frame_count++;
		ctx_.real_time += real_dt;
	};

	if (ctx_.window.GetSetting(WindowSetting::Minimized)) {
		ctx_.audio.Update();
		end_frame();
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

		ctx_.unscaled_game_time += unscaled_dt;
		ctx_.game_time += ctx_.dt;
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

	end_frame();
}

} // namespace ptgn
