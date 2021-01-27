#pragma once

#include <cstdlib> // std::size_t
#include <cstdint> // std::int32_t, etc
#include <cassert> // assert

#include "core/Scene.h"

#include "ecs/ECS.h"

#include "renderer/Window.h"
#include "renderer/Renderer.h"

#include "utils/Debug.h"
#include "utils/math/Vector2.h"

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
// Default FPS.
constexpr std::size_t FPS = 60;

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
	static void Start(const char* title = internal::WINDOW_TITLE, int width = internal::WINDOW_WIDTH, int height = internal::WINDOW_HEIGHT, std::size_t fps = internal::FPS, int x = internal::WINDOW_X, int y = internal::WINDOW_Y, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0) {
		instance_ = new T{};
		auto& engine = GetInstance();
		engine.window_size_ = { width, height };
		engine.window_position_ = { x, y };
		engine.fps_ = fps;
		engine.inverse_fps_ = 1.0 / static_cast<double>(engine.fps_);
		engine.window_title_ = title;
		engine.running_ = true;
		LOG("Initializing SDL...");
		engine.InitSDL(window_flags, renderer_flags);
		LOG("All SDL components fully initialized");
		engine.InitInternals();
		engine.Init();
		engine.Loop();
		engine.Clean();
	}
	static void Quit();
	static Window& GetWindow();
	static Renderer& GetRenderer();
	static V2_int ScreenSize();
	static int ScreenWidth();
	static int ScreenHeight();
	static std::pair<Window, Renderer> GenerateWindow(const char* window_title, V2_int window_position, V2_int window_size, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0);
	static void Delay(std::uint32_t milliseconds);
	static std::size_t FPS();
	static double InverseFPS();
protected:
	Scene scene;
private:
	static Engine& GetInstance() {
		assert(instance_ != nullptr && "Engine instance not created yet");
		return *instance_;
	}
	void InitInternals();
	void InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags);
	void InputHandlerUpdate();
	void ResetWindowColor();
	void Clean();
	std::uint32_t GetTicks();
	// Main game loop.
	void Loop() {
		const std::uint32_t delay = static_cast<std::uint32_t>(1000.0 * inverse_fps_);
		std::uint32_t start;
		std::uint32_t time;
		while (running_) {
			start = GetTicks();
			InputHandlerUpdate();
			Update();
			renderer_.Clear();
			ResetWindowColor();
			// Render everything here.
			Render();
			renderer_.Present();
			time = GetTicks() - start;
			if (delay > time) { // cap frame time at an FPS
				Delay(delay - time);
			}
		}
	}
	static Engine* instance_;
	Window window_{ nullptr };
	Renderer renderer_{ nullptr };
	bool running_{ false };
	V2_int window_size_{ 0, 0 };
	V2_int window_position_{ 0, 0 };
	int sdl_init_{ 1 };
	int ttf_init_{ 1 };
	const char* window_title_{ "" };
	std::size_t fps_{ 0 };
	double inverse_fps_{ 0.0 };
};

} // namespace engine
