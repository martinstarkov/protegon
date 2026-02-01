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
#include "renderer/resources/vertex.h"

class Renderer {
public:
};

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

	impl::gl::GLContext gl_{ window };

	auto quad_shader = gl_.GetShader("quad");

	std::uint32_t batch_capacity{ 4000 };
	std::uint32_t vertex_capacity{ 4 * batch_capacity };
	std::uint32_t index_capacity{ 6 * batch_capacity };

	using Index = std::uint32_t;

	auto ebo = gl_.CreateElementBuffer(nullptr, index_capacity, sizeof(Index), GL_DYNAMIC_DRAW);
	auto vbo =
		gl_.CreateVertexBuffer(nullptr, vertex_capacity, sizeof(impl::Vertex), GL_DYNAMIC_DRAW);

	auto vao = gl_.CreateVertexArray(vbo, impl::Vertex::GetLayout(), ebo);

	Renderer renderer;

	bool running = true;

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		// renderer.Clear(scene, { 0, 0, 0, 1 });

		// renderer.DrawRect(scene, { 0, 0 }, { 0.4f, 0.25f }, { 1, 0, 0, 0.85f });

		SDL_GL_SwapWindow(window);
	}

	SDL_Quit();
	return 0;
}
