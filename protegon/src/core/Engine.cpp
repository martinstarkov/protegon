#include "Engine.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "core/Window.h"
#include "debugging/DebugRenderer.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/Colors.h"
#include "renderer/TextureManager.h"
#include "renderer/text/FontManager.h"
#include "event/InputHandler.h"
#include "event/EventHandler.h"

namespace ptgn {

void Engine::DelayImpl(milliseconds duration) {
	SDL_Delay(static_cast<std::uint32_t>(duration.count()));
}

void Engine::Update() {
	InputHandler::Update();
	EventHandler::Update();
	
	if (!running_) return;

	SceneManager::UpdateActiveScene();
	
	if (!running_) return;
	
	ScreenRenderer::SetDrawColor(colors::DEFAULT_BACKGROUND_COLOR);
	
	ScreenRenderer::Clear();

	ScreenRenderer::SetDrawColor(colors::DEFAULT_DRAW_COLOR);
	
	SceneManager::RenderActiveScene();

	DebugRenderer<WorldRenderer>::Render();
	DebugRenderer<ScreenRenderer>::Render();

	ScreenRenderer::Present();

	DebugRenderer<WorldRenderer>::ResolveQueuedDelays();
	DebugRenderer<ScreenRenderer>::ResolveQueuedDelays();

	SceneManager::UnloadFlaggedScenes();
}

void Engine::Destroy() {
	// Destroy all engine subsystems.
	ScreenRenderer::Destroy();
	Window::Destroy();
	FontManager::Destroy();
	TextureManager::Destroy();
	// Quit SDL subsystems.
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
	// Reset engine state.
	init_ = false;
	fps_ = 0;
}

void Engine::Loop() {
	while (running_) {
		auto loop_start{ application_timer_.Elapsed<time>() };

		Update();

		auto loop_time{ application_timer_.Elapsed<time>() - loop_start };

		time difference{ frame_time_ - loop_time };

		if (difference > time{ 0 }) {
			Delay(std::chrono::duration_cast<milliseconds>(difference));
		}
	}
}

void Engine::SetFPS(std::size_t fps) {
	assert(fps > 0);
	auto& instance{ GetInstance() };
	instance.fps_ = fps;
	instance.frame_time_ = std::chrono::duration_cast<time>(
		std::chrono::duration<double, std::ratio<1, 1>>{ 1.0 / static_cast<double>(instance.fps_) }
	);
}

void Engine::InitSDLComponents() {
	auto sdl_flags{ 
		SDL_INIT_AUDIO |
		SDL_INIT_EVENTS |
		SDL_INIT_TIMER |
		SDL_INIT_VIDEO
	};
	assert(!SDL_WasInit(sdl_flags) && 
		   "Cannot initialize SDL components more than one time");
	auto sdl_init{ SDL_Init(sdl_flags) };
	if (sdl_init != 0) {
		std::cerr << "SDL_Init: " << SDL_GetError() << std::endl;
		abort();
	}
	auto img_flags{ IMG_INIT_PNG | IMG_INIT_JPG	};
	auto img_init{ IMG_Init(img_flags) };
	if ((img_init & img_flags) != img_flags) {
		std::cerr << "IMG_Init: Failed to init required png and jpg support!" << std::endl;
		std::cerr << "IMG_Init: " << IMG_GetError() << std::endl;
		abort();
	}
	auto ttf_init{ TTF_Init() };
	if (ttf_init == -1) {
		std::cerr << "TTF_Init: " << TTF_GetError() << std::endl;
		abort();
	}
	init_ = true;
}

void Engine::CreateDisplay(const char* window_title,
						   const V2_int& window_position,
						   const V2_int& window_size,
						   std::uint32_t window_flags, 
						   std::uint32_t renderer_flags) {
	auto& engine{ GetInstance() };
	assert(engine.init_ && "Cannot generate window before initializing SDL components");
	auto& window{ Window::Init(window_title,
							   window_position,
							   window_size,
							   window_flags) 
	};
	assert(window.IsValid() && "SDL failed to create window");
	auto& renderer{ ScreenRenderer::Init(window, -1, renderer_flags) };
	assert(renderer.IsValid() && "SDL failed to create renderer");
}

void Engine::Quit() {
	GetInstance().running_ = false;
}

} // namespace ptgn