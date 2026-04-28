#include "app/application.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <memory>
#include <string_view>
#include <utility>

#include "app/application_config.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "application_context.h"
#include "application_state.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/event/window_event.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/audio/audio_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

Application::Application(const ApplicationConfig& config) : ctx_{ config } {}

Application::Application(std::string_view title) :
	Application{ ApplicationConfig{ .window{ .title{ title } } } } {}

Application::Application(std::string_view title, V2_int window_size) :
	Application{ ApplicationConfig{ .window{ .title{ title }, .size{ window_size } } } } {}

Application::~Application() noexcept = default;

void Application::EnterMainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	ctx_.window.SetSetting(WindowSetting::Shown);
	ctx_.running = true;

	ctx_.renderer.UpdateDisplayViewport(true);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(
		[](void* application) {
			auto& app{ *static_cast<Application*>(application) };

			app.Update();

			if (!app.running_) {
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
			ctx_.renderer.OnWindowResize(size);
		});
		if (!dispatch_scene_events) {
			continue;
		}
		for (const auto& scene : ctx_.scene_manager.GetScenes()) {
			if (scene->IsAwaitingTransitionDelay()) {
				continue;
			}
			scene->InternalOnEvent(event);
		}
	}
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
	// Calculate time elapsed during previous frame.
	ctx_.dt = end - start;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.

	secondsf max_dt{ 1.0f / ctx_.fps };

	if (ctx_.dt > max_dt) {
		// TODO: Instead of clamping, consider using an accumulator to update multiple times if dt
		// is large (such as in Debug mode).
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

	using enum ApplicationState;

	ctx_.running = ctx_.window.PollEvents();

	if (ctx_.window.GetSetting(WindowSetting::Minimized)) {
		ctx_.audio.Update();
		ctx_.debug.PostUpdate();
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
		layer->OnUpdate();
	}

	ctx_.audio.Update();

	ctx_.debug.PostUpdate();

	if (scene_rendering) {
		ctx_.renderer.BeginFrame();
		ctx_.scene_manager.Draw();
		ctx_.renderer.EndFrame();
	}

	for (const auto& layer : ctx_.layers) {
		layer->OnRender();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ctx_.window.SwapBuffers();

	end = std::chrono::steady_clock::now();
	ctx_.frame_count++;
}

} // namespace ptgn