#include <SDL3/SDL.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "platform/window/window.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/renderer.h"
#include "renderer/resources/vertex.h"

int main() {
	using namespace ptgn;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return 1;
	}

	// OpenGL 3.3 core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int W = 1280, H = 720;

	ptgn::Window window{ "Title", { 1280, 720 } };
	window.SetSetting(WindowSetting::Shown);

	Renderer renderer{ window };

	bool running = true;

	auto window_size = window.GetSize();

	auto scene_texture =
		renderer.gl_->CreateTexture(nullptr, GL_RGBA, GL_UNSIGNED_INT, window_size, GL_RGBA);

	auto scene_fbo = renderer.gl_->CreateFrameBuffer(scene_texture);

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		renderer.FrameStart();

		// renderer.Clear(scene, { 0, 0, 0, 1 });

		// renderer.DrawRect(scene, { 0, 0 }, { 0.4f, 0.25f }, { 1, 0, 0, 0.85f });

		renderer.Present();

		SDL_GL_SwapWindow(window);
	}

	SDL_Quit();
	return 0;
}
