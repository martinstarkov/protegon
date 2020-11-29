#pragma once

#include <cstdint>
#include <cassert>

#include "core/Scene.h"

#include "ecs/ECS.h"

#include "renderer/Window.h"
#include "renderer/Renderer.h"

#include "utils/Defines.h"
#include "utils/Vector2.h"

#include "renderer/TextureManager.h"

namespace engine {

namespace internal {

#define CENTERED 0x2FFF0000u|0 // SDL_WINDOWPOS_CENTERED

// Default window title.
constexpr const char* WINDOW_TITLE = "Unknown Title";
// Default window position centered.
constexpr int WINDOW_X = CENTERED;
constexpr int WINDOW_Y = CENTERED;
// Default window width.
constexpr int WINDOW_WIDTH = 600;
// Default window height.
constexpr int WINDOW_HEIGHT = 480;

} // namespace internal

class Engine {
public:
	// Function which is called upon starting the engine.
	virtual void Init() {}
	// Function which is called each frame of the engine loop.
	virtual void Update() {}
	// Called after the update function, each frame of the engine loop.
	virtual void Render() {}
	
	template <typename T>
	static void Start(const char* title = internal::WINDOW_TITLE, int width = internal::WINDOW_WIDTH, int height = internal::WINDOW_HEIGHT, int x = internal::WINDOW_X, int y = internal::WINDOW_Y, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0) {
		window_size_ = { width, height };
		window_position_ = { x, y };
		window_title_ = title;
		running_ = true;
		LOG("Initializing SDL...");
		InitSDL(window_flags, renderer_flags);
		LOG("All SDL components fully initialized");
		InitInternals();
		auto& engine = GetInstance<T>();
		engine.Init();
		Loop<T>(engine);
		Clean();
	}
	static void Quit();
	static Window& GetWindow();
	static Renderer& GetRenderer();
	static V2_int ScreenSize();
	static int ScreenWidth();
	static int ScreenHeight();
	static std::pair<Window, Renderer> GenerateWindow(const char* window_title, V2_int window_position, V2_int window_size, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0);
protected:
	Scene scene;
private:
	static void InitInternals();
	static void InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags);
	static void InputHandlerUpdate();
	static void ResetWindowColor();
	static void Clean();
	static void Delay(std::uint32_t milliseconds);
	static std::uint32_t GetTicks();
	// Singleton.
	template <typename T>
	static T& GetInstance() {
		if (!instance_) {
			instance_ = new T{};
		}
		return *static_cast<T*>(instance_);
	}
	// Main game loop.
	template <typename T>
	static void Loop(T& engine) {
		const std::uint32_t delay = 1000 / FPS;
		std::uint32_t start;
		std::uint32_t time;
		while (running_) {
			start = GetTicks();
			InputHandlerUpdate();
			engine.Update();
			renderer_.Clear();
			ResetWindowColor();
			// Render everything here.
			engine.Render();
			renderer_.Present();
			time = GetTicks() - start;
			if (delay > time) { // cap frame time at an FPS
				Delay(delay - time);
			}
		}
	}
	static Engine* instance_;
	static int sdl_init;
	static int ttf_init;
	static Window window_;
	static Renderer renderer_;
	static bool running_;
	static V2_int window_size_;
	static V2_int window_position_;
	static const char* window_title_;
};

} // namespace engine
